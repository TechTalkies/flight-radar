#include "AircraftManager.h"
#include <vector>

constexpr int SCREEN_SIZE = 240;
constexpr int SCREEN_SIZE_DIV_2 = (SCREEN_SIZE / 2);

#include <ArduinoJson.h>

enum ScreenMode
{
    SCREEN_RADAR,
    SCREEN_DETAILS
};

ScreenMode currentScreen = SCREEN_RADAR;

int selectedAircraftIndex = 0;
std::vector<TrackedAircraft *> visibleAircraft;

static const uint32_t AircraftColours[] =
    {
        lgfx::color888(0, 255, 128),   // Radar green
        lgfx::color888(0, 220, 255),   // Cyan
        lgfx::color888(0, 128, 255),   // Electric blue
        lgfx::color888(100, 200, 255), // Sky blue
        lgfx::color888(160, 130, 255), // Soft violet
        lgfx::color888(220, 80, 255),  // Magenta
        lgfx::color888(255, 50, 150),  // Hot pink
        lgfx::color888(255, 60, 60),   // Coral red
        lgfx::color888(255, 120, 0),   // Burnt orange
        lgfx::color888(255, 190, 0),   // Amber
        lgfx::color888(255, 235, 0),   // Neon yellow
        lgfx::color888(180, 255, 0),   // Acid lime
        lgfx::color888(80, 255, 180),  // Mint
        lgfx::color888(0, 200, 180),   // Teal
        lgfx::color888(50, 180, 255),  // Cornflower
        lgfx::color888(255, 160, 100), // Peach
        lgfx::color888(255, 210, 160), // Warm cream
        lgfx::color888(140, 255, 140), // Pale green
        lgfx::color888(200, 160, 255), // Lavender
        lgfx::color888(255, 120, 200)  // Pink
};

void AircraftManager::Initialise()
{
    // get centre point + radius
    lat = configServer.GetStoredString("latitude").toDouble();
    lon = configServer.GetStoredString("longitude").toDouble();
    rad = configServer.GetStoredString("radius").toDouble();

    // configuration
    const String renderText = configServer.GetStoredString("infotext");
    const String renderTris = configServer.GetStoredString("triangle");
    if (!renderText.isEmpty())
        displayInfoText = renderText == "true" ? true : false;
    if (!renderTris.isEmpty())
        displayTriangles = renderTris == "true" ? true : false;

    // calculate how often we can call OpenSky API before being rate limited
    constexpr int MS_PER_DAY = 24 * 60 * 60 * 1000;
    constexpr int ANONYMOUS_TOKENS_PER_DAY = 400;
    constexpr int AUTHED_TOKENS_PER_DAY = 4000;
    constexpr int TOKEN_BUFFER = 3;
    int dailyRequestBudget = ANONYMOUS_TOKENS_PER_DAY - TOKEN_BUFFER; // non-authed tokens minus buffer

    const String token = authHandler.GetValidToken(configServer.GetStoredString("opensky-id"), configServer.GetStoredString("opensky-secret"));
    if (!token.isEmpty())
        dailyRequestBudget = AUTHED_TOKENS_PER_DAY - TOKEN_BUFFER; // authed tokens minus buffer

    fetchInterval = MS_PER_DAY / dailyRequestBudget;
}

void AircraftManager::Update()
{
    unsigned long now = millis();

    // fetch cycle
    if (now - lastFetch >= fetchInterval)
    {
        lastFetch = now;

        // auth
        const String token = authHandler.GetValidToken(
            configServer.GetStoredString("opensky-id"),
            configServer.GetStoredString("opensky-secret"));

        std::vector<std::pair<String, String>> headers = {};
        if (!token.isEmpty())
            headers.push_back({"Authorization", "Bearer " + token});

        // request
        HttpResult result = http.Get(
            "https://opensky-network.org/api/states/all",
            {{"lamin", String(lat - rad)},
             {"lamax", String(lat + rad)},
             {"lomin", String(lon - rad)},
             {"lomax", String(lon + rad)}},
            headers);

        // If request failed, skip this update
        if (!result.success)
        {
            Serial.print("[WARN] OpenSky API request failed: ");
            Serial.println(result.errorMessage);
            return;
        }

        // track
        JsonDocument doc;
        deserializeJson(doc, result.response);
        auto aircraft = JsonParser::ParseArray<Aircraft>(doc["states"]);
        now = millis(); // override with post-parse timestamp

        for (auto &ac : aircraft)
        {
            auto it = trackedAircraft.find(ac.icao24);
            if (it == trackedAircraft.end())
                trackedAircraft.emplace(ac.icao24, TrackedAircraft{ac, now});
            else
                it->second.Update(ac, now);
        }

        // remove any planes that disappeared from the feed
        for (auto it = trackedAircraft.begin(); it != trackedAircraft.end();)
        {
            bool aircraftPresent = std::any_of(aircraft.begin(), aircraft.end(), [&](const Aircraft &ac)
                                               { return ac.icao24 == it->first; });
            if (!aircraftPresent)
                it = trackedAircraft.erase(it);
            else
                ++it;
        }
    }
}

void AircraftManager::DrawDetails(LGFX_Sprite &backbuffer)
{
    backbuffer.fillScreen(TFT_BLACK);

    constexpr int CENTRE = SCREEN_SIZE_DIV_2 - 1;
    constexpr int OUTER = SCREEN_SIZE_DIV_2 - 5;

    // Outer circular frame
    backbuffer.drawCircle(
        CENTRE,
        CENTRE,
        OUTER,
        lgfx::color888(0, 200, 0));

    if (visibleAircraft.empty())
    {
        backbuffer.setTextColor(lgfx::color888(0, 255, 0));
        backbuffer.setTextDatum(textdatum_t::middle_center);
        backbuffer.drawString("NO AIRCRAFT", CENTRE, CENTRE);
        return;
    }

    TrackedAircraft &tracked =
        *visibleAircraft[selectedAircraftIndex];

    // Aircraft icon
    DrawAircraftTriangle(
        backbuffer,
        CENTRE,
        45,
        tracked,
        false);

    backbuffer.setTextColor(lgfx::color888(0, 255, 0));
    backbuffer.setTextDatum(textdatum_t::middle_center);

    // Callsign
    String callsign = tracked.state.callsign;
    callsign.trim();

    backbuffer.setTextSize(2);
    backbuffer.drawString(
        callsign,
        CENTRE,
        75);

    backbuffer.setTextSize(1);

    int y = 105;
    constexpr int LINE = 20;

    backbuffer.drawString(
        "ALT " + String((int)tracked.state.baroAltitude) + " m",
        CENTRE,
        y);

    y += LINE;

    backbuffer.drawString(
        "SPD " + String((int)tracked.state.velocity) + " m/s",
        CENTRE,
        y);

    y += LINE;

    backbuffer.drawString(
        "HDG " + String((int)tracked.state.trueTrack) + " deg",
        CENTRE,
        y);

    y += LINE;

    backbuffer.drawString(
        tracked.state.icao24,
        CENTRE,
        y);

    y += 30;

    backbuffer.setTextColor(lgfx::color888(0, 100, 0));

    backbuffer.drawString(
        "< Rotate >",
        CENTRE,
        y);

    y += 18;

    backbuffer.drawString(
        "Click to return",
        CENTRE,
        y);
}

void AircraftManager::EncoderClick()
{
    currentScreen =
        (currentScreen == SCREEN_RADAR)
            ? SCREEN_DETAILS
            : SCREEN_RADAR;
}

void AircraftManager::Draw(LGFX_Sprite &backbuffer)
{
    visibleAircraft.clear();

    for (auto &[icao, tracked] : trackedAircraft)
    {
        if (tracked.state.onGround)
            continue;

        visibleAircraft.push_back(&tracked);
    }

    if (currentScreen == SCREEN_DETAILS)
    {
        DrawDetails(backbuffer);
        return;
    }

    DrawRadarCircles(backbuffer);

    visibleAircraft.clear();

    for (auto &[icao, tracked] : trackedAircraft)
    {
        if (tracked.state.onGround)
            continue;

        visibleAircraft.push_back(&tracked);
    }

    if (!visibleAircraft.empty())
    {
        selectedAircraftIndex =
            constrain(
                selectedAircraftIndex,
                0,
                (int)visibleAircraft.size() - 1);
    }
    else
    {
        selectedAircraftIndex = 0;
    }

    for (size_t i = 0; i < visibleAircraft.size(); i++)
    {
        TrackedAircraft &tracked = *visibleAircraft[i];

        tracked.Tick();

        auto [predLat, predLon] = tracked.GetDisplayPosition();
        auto [x, y] = ProjectCoordinateToScreen(predLat, predLon);

        bool selected =
            ((int)i == selectedAircraftIndex);

        if (displayInfoText)
            DrawAircraftInfo(backbuffer, x, y, tracked);

        if (displayTriangles)
            DrawAircraftTriangle(
                backbuffer,
                x,
                y,
                tracked,
                selected);
        else
        {
            backbuffer.fillCircle(
                x,
                y,
                selected ? 5 : 3,
                selected
                    ? lgfx::color888(0, 0, 255)
                    : lgfx::color888(0, 255, 0));
        }
    }
}

uint32_t AircraftManager::GetAircraftColour(
    const TrackedAircraft &tracked) const
{
    uint32_t hash = 0;

    for (char c : tracked.state.icao24)
        hash = hash * 31 + c;

    return AircraftColours[hash % (sizeof(AircraftColours) /
                                   sizeof(AircraftColours[0]))];
}

void AircraftManager::SelectNextAircraft()
{
    if (visibleAircraft.empty())
        return;

    selectedAircraftIndex++;

    if (selectedAircraftIndex >= visibleAircraft.size())
        selectedAircraftIndex = 0;

    Serial.print("Aircraft count: ");
    Serial.println(visibleAircraft.size());

    Serial.print("Selected index: ");
    Serial.println(selectedAircraftIndex);
}

void AircraftManager::SelectPreviousAircraft()
{
    if (visibleAircraft.empty())
        return;

    selectedAircraftIndex--;

    if (selectedAircraftIndex < 0)
        selectedAircraftIndex =
            visibleAircraft.size() - 1;

    Serial.print("Selected previous aircraft, index: ");
    Serial.println(selectedAircraftIndex);
}

void AircraftManager::DrawRadarCircles(LGFX_Sprite &backbuffer) const
{
    constexpr int CENTRE = SCREEN_SIZE_DIV_2 - 1;
    constexpr int OUTER = SCREEN_SIZE_DIV_2 - 5;

    backbuffer.drawCircle(CENTRE, CENTRE, OUTER, lgfx::color888(0, 200, 0));
    backbuffer.drawCircle(CENTRE, CENTRE, (OUTER / 3) * 2, lgfx::color888(0, 64, 0));
    backbuffer.drawCircle(CENTRE, CENTRE, OUTER / 3, lgfx::color888(0, 32, 0));
}

std::pair<int, int> AircraftManager::ProjectCoordinateToScreen(float predLat, float predLon) const
{
    const float dLon = predLon - lon;
    const float dLat = predLat - lat;

    const float normLon = (dLon + rad) / (2.0f * rad);
    const float normLat = (dLat + rad) / (2.0f * rad);

    const int x = static_cast<int>(normLon * SCREEN_SIZE);
    const int y = static_cast<int>(SCREEN_SIZE - (normLat * SCREEN_SIZE));

    return {x, y};
}

void AircraftManager::DrawAircraftInfo(LGFX_Sprite &backbuffer, int x, int y, const TrackedAircraft &tracked) const
{
    // const int lineHeight = tft.fontHeight() + 1;

    backbuffer.setTextSize(1);
    backbuffer.setTextColor(lgfx::color888(0, 128, 0));
    backbuffer.drawString(tracked.state.callsign, x + 10, y + 15);
    // backbuffer.drawString(String(tracked.state.velocity) + "m/s", x + 5, y + 5 + lineHeight);
    // backbuffer.drawString(String(tracked.state.baroAltitude) + "m", x + 5, y + 5 + lineHeight * 2);
}

void AircraftManager::DrawAircraftTriangle(LGFX_Sprite &backbuffer, int x, int y, const TrackedAircraft &tracked) const
{
    const float dx = std::sin(radians(tracked.state.trueTrack));
    const float dy = -std::cos(radians(tracked.state.trueTrack));
    const float px = -dy;
    const float py = dx;

    constexpr float TRIANGLE_LENGTH = 6.0f;
    constexpr float TRIANGLE_WIDTH = 3.0f;

    const float tipX = x + dx * TRIANGLE_LENGTH;
    const float tipY = y + dy * TRIANGLE_LENGTH;
    const float leftX = x - dx * TRIANGLE_LENGTH * 0.5f + px * TRIANGLE_WIDTH * 0.5f;
    const float leftY = y - dy * TRIANGLE_LENGTH * 0.5f + py * TRIANGLE_WIDTH * 0.5f;
    const float rightX = x - dx * TRIANGLE_LENGTH * 0.5f - px * TRIANGLE_WIDTH * 0.5f;
    const float rightY = y - dy * TRIANGLE_LENGTH * 0.5f - py * TRIANGLE_WIDTH * 0.5f;

    backbuffer.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, lgfx::color888(0, 255, 0));
}

void AircraftManager::DrawAircraftTriangle(
    LGFX_Sprite &backbuffer,
    int x,
    int y,
    const TrackedAircraft &tracked,
    bool selected) const
{
    const float dx = std::sin(radians(tracked.state.trueTrack));
    const float dy = -std::cos(radians(tracked.state.trueTrack));

    const float px = -dy;
    const float py = dx;

    const uint16_t colour =
        selected
            ? lgfx::color888(255, 255, 255) // White
            : GetAircraftColour(tracked);

    constexpr float BODY_FRONT = 8.0f;
    constexpr float BODY_REAR = 6.0f;

    constexpr float WING_SPAN = 8.0f;
    constexpr float WING_SWEEP = 4.0f;

    // Fuselage endpoints
    const float noseX = x + dx * BODY_FRONT;
    const float noseY = y + dy * BODY_FRONT;

    const float tailX = x - dx * BODY_REAR;
    const float tailY = y - dy * BODY_REAR;

    // Rounded fuselage (implemented as 3 parallel lines)
    for (int i = -1; i <= 1; i++)
    {
        backbuffer.drawLine(
            noseX + px * i,
            noseY + py * i,
            tailX + px * i,
            tailY + py * i,
            colour);
    }

    // Rounded ends
    backbuffer.fillCircle(noseX, noseY, 1, colour);
    backbuffer.fillCircle(tailX, tailY, 1, colour);

    // Wing triangle
    const float wingCenterX = x + dx * 2.5f;
    const float wingCenterY = y + dy * 2.5f;

    const float leftWingX =
        wingCenterX + px * WING_SPAN - dx * WING_SWEEP;

    const float leftWingY =
        wingCenterY + py * WING_SPAN - dy * WING_SWEEP;

    const float rightWingX =
        wingCenterX - px * WING_SPAN - dx * WING_SWEEP;

    const float rightWingY =
        wingCenterY - py * WING_SPAN - dy * WING_SWEEP;

    backbuffer.fillTriangle(
        wingCenterX,
        wingCenterY,

        leftWingX,
        leftWingY,

        rightWingX,
        rightWingY,

        colour);
}