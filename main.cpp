// enhanced_cityscape_with_clock_and_children.c
// Compile with: gcc enhanced_cityscape_with_clock_and_children.c -lGL -lGLU -lglut -lm -o city
// Runs a 2D scene with a fast simulated clock: 1 real minute = 12 simulated hours (speed multiplier = 720)

#include <GL/glut.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#define PI 3.14159265358979323846

double sim_seconds_d = 0.0;  // global simulated seconds of day


// Global random offsets for clouds
float cloudOffsetsX[8];
float cloudOffsetsY[8];
float cloudScales[8];

// Snow system variables
bool snowEnabled = false;
double snowStartTime = 0.0;
const int MAX_SNOWFLAKES = 8000; // Increased from 300 to 8000

struct Snowflake {
    float x;
    float y;
    float speed;
    float sway;
    bool active;
};

Snowflake snowflakes[MAX_SNOWFLAKES];
float groundSnowLevel = 0.0f; // Snow accumulation on ground (0 to 1)



// Window / scene logical size (keep large so your previous coordinates still work)
const int SCENE_W = 2623;
const int SCENE_H = 1631;

// Simulation speed: 1 real second -> SIM_SPEED simulated seconds
// Explanation: 12 simulated hours = 43200 simulated seconds correspond to 60 real seconds
// So SIM_SPEED = 86400 / 60 = 1440
const double SIM_SPEED = 1440.0;

// Animation fps (~30)
const int TIMER_MS = 33;

// Global ball position
float ballx = 1500.0f;  // starting X
float bally = 100.0f;   // starting Y

// Forward
void Draw();
void init(void);
void drawRealisticCar(float x, float y, float w, float h, float r, float g, float b);
void drawRealisticBus(float x, float y, float w, float h);

// ---------- Helpers ----------
void rect(float x1, float y1, float x2, float y2)
{
    glBegin(GL_POLYGON);
    glVertex2f(x1,y1);
    glVertex2f(x2,y1);
    glVertex2f(x2,y2);
    glVertex2f(x1,y2);
    glEnd();
}

// Midpoint circle algorithm for glow
void glowCircle(float cx, float cy, float radius, float r, float g, float b, float alpha)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // draw concentric circles outward for soft glow
    for(int rad = (int)radius; rad <= (int)(radius*2.0f); rad+=2)
    {
        float a = alpha * (1.0f - (float)(rad-radius)/(radius)); // fade outward
        glColor4f(r,g,b,a);
        glBegin(GL_POINTS);
        int x = 0;
        int y = rad;
        int d = 1 - rad;
        while(x <= y)
        {
            glVertex2f(cx+x, cy+y);
            glVertex2f(cx-x, cy+y);
            glVertex2f(cx+x, cy-y);
            glVertex2f(cx-x, cy-y);
            glVertex2f(cx+y, cy+x);
            glVertex2f(cx-y, cy+x);
            glVertex2f(cx+y, cy-x);
            glVertex2f(cx-y, cy-x);
            if(d < 0)
            {
                d += 2*x + 3;
            }
            else
            {
                d += 2*(x-y) + 5;
                y--;
            }
            x++;
        }
        glEnd();
    }

    glDisable(GL_BLEND);
}

// Bresenham's Line Drawing Algorithm
void bresenhamLine(int x0, int y0, int x1, int y1, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_POINTS);

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        glVertex2i(x0, y0);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    glEnd();
}

// Initialize snowflakes
void initSnowflakes() {
    for (int i = 0; i < MAX_SNOWFLAKES; i++) {
        snowflakes[i].x = (rand() % (SCENE_W * 2)) - SCENE_W/2; // Spread beyond screen
        snowflakes[i].y = rand() % (SCENE_H + 500); // Randomize entire height
        snowflakes[i].speed = 1.5f + (rand() % 250) / 100.0f; // Speed: 1.5 to 4.0
        snowflakes[i].sway = (rand() % 40 - 20) / 100.0f; // More sway variation
        snowflakes[i].active = true;
    }
}

// Update snowflakes
void updateSnowflakes(double deltaTime) {
    if (!snowEnabled) return;

    // Calculate snow accumulation (full coverage after 1-2 hours of sim time)
    double snowDuration = sim_seconds_d - snowStartTime;
    double hoursOfSnow = snowDuration / 3600.0; // Convert to hours
    groundSnowLevel = fmin(hoursOfSnow / 1.5, 1.0); // Gradually reach 1.0 over 1.5 hours

    for (int i = 0; i < MAX_SNOWFLAKES; i++) {
        if (snowflakes[i].active) {
            // Move snowflake down - using slower, consistent speed
            // Don't multiply by deltaTime since we're updating every frame
            snowflakes[i].y -= snowflakes[i].speed;

            // Add horizontal sway with slight variation
            snowflakes[i].x += snowflakes[i].sway;

            // Add subtle turbulence for more natural movement
            if (rand() % 100 < 5) { // 5% chance each frame
                snowflakes[i].sway += (rand() % 10 - 5) / 200.0f;
                // Clamp sway to prevent excessive drift
                if (snowflakes[i].sway > 0.3f) snowflakes[i].sway = 0.3f;
                if (snowflakes[i].sway < -0.3f) snowflakes[i].sway = -0.3f;
            }

            // Reset if off screen (with buffer to prevent loss)
            if (snowflakes[i].y < -10) {
                // Randomize position more to prevent vertical lines
                snowflakes[i].x = (rand() % (SCENE_W + 400)) - 200; // Can start slightly off-screen
                snowflakes[i].y = SCENE_H + (rand() % 300); // More randomized starting height
                snowflakes[i].speed = 1.5f + (rand() % 250) / 100.0f; // 1.5 to 4.0
                snowflakes[i].sway = (rand() % 40 - 20) / 100.0f; // -0.2 to 0.2
            }

            // Wrap horizontally with smooth transition
            if (snowflakes[i].x < -50) snowflakes[i].x = SCENE_W + 50;
            if (snowflakes[i].x > SCENE_W + 50) snowflakes[i].x = -50;
        }
    }
}

// Draw snowflakes using Bresenham's algorithm
void drawSnowflakes() {
    if (!snowEnabled) return;

    for (int i = 0; i < MAX_SNOWFLAKES; i++) {
        if (snowflakes[i].active) {
            int x = (int)snowflakes[i].x;
            int y = (int)snowflakes[i].y;

            // Draw snowflake as small lines forming a star pattern
            // Using Bresenham's line algorithm
            bresenhamLine(x - 2, y, x + 2, y, 1.0f, 1.0f, 1.0f);
            bresenhamLine(x, y - 2, x, y + 2, 1.0f, 1.0f, 1.0f);
            bresenhamLine(x - 1, y - 1, x + 1, y + 1, 0.9f, 0.9f, 1.0f);
            bresenhamLine(x - 1, y + 1, x + 1, y - 1, 0.9f, 0.9f, 1.0f);
        }
    }
}

// Draw snow accumulation on ground
void drawGroundSnow() {
    if (!snowEnabled || groundSnowLevel <= 0.0f) return;

    // Semi-transparent white overlay on ground
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float alpha = groundSnowLevel * 0.7f; // Max 70% opacity
    glColor4f(1.0f, 1.0f, 1.0f, alpha);

    // Draw snow ONLY on grass areas, avoiding roads and children playing area
    // Bottom grass area (below road) - Y 0 to 150
    rect(0, 0, SCENE_W, 150);

    // Top grass area (above road) - Y 265 to 300, but exclude children area (X 1250-1900)
    rect(0, 265, 1250, 300); // Left side grass
    rect(1900, 265, SCENE_W, 300); // Right side grass

    // Add some texture using random Bresenham lines only on grass areas
    if (groundSnowLevel > 0.3f) {
        srand(12345); // Fixed seed for consistent pattern
        for (int i = 0; i < 100; i++) {
            int x1 = rand() % SCENE_W;
            int y1 = rand() % 150; // Only bottom grass area

            // Skip if in children area
            if (y1 > 265 && x1 > 1250 && x1 < 1900) continue;

            int x2 = x1 + (rand() % 10 - 5);
            int y2 = y1 + (rand() % 3);

            glColor4f(1.0f, 1.0f, 1.0f, alpha * 0.3f);
            bresenhamLine(x1, y1, x2, y2, 1.0f, 1.0f, 1.0f);
        }

        // Add texture to top grass areas too
        for (int i = 0; i < 50; i++) {
            int x1 = rand() % 1250; // Left grass
            int y1 = 265 + rand() % 35;
            int x2 = x1 + (rand() % 10 - 5);
            int y2 = y1 + (rand() % 3);

            glColor4f(1.0f, 1.0f, 1.0f, alpha * 0.3f);
            bresenhamLine(x1, y1, x2, y2, 1.0f, 1.0f, 1.0f);
        }

        for (int i = 0; i < 50; i++) {
            int x1 = 1900 + rand() % (SCENE_W - 1900); // Right grass
            int y1 = 265 + rand() % 35;
            int x2 = x1 + (rand() % 10 - 5);
            int y2 = y1 + (rand() % 3);

            glColor4f(1.0f, 1.0f, 1.0f, alpha * 0.3f);
            bresenhamLine(x1, y1, x2, y2, 1.0f, 1.0f, 1.0f);
        }
    }

    glDisable(GL_BLEND);
}


void wheel(float cx, float cy, float r)
{
    int i;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for(i=0; i<=36; i++)
    {
        float a = (2.0f*PI*i)/36.0f;
        glVertex2f(cx + cos(a)*r, cy + sin(a)*r);
    }
    glEnd();
}

// --- Stylized simple car (matches uploaded image) ---
void drawStylizedCar(float x, float y, float w, float h, float r, float g, float b) {
    // Body
    glColor3f(r, g, b);
    rect(x, y, x + w, y + h);

    // Window (black tint)
    glColor3f(0.05f, 0.05f, 0.05f);
    rect(x + w * 0.2f, y + h * 0.4f, x + w * 0.8f, y + h * 0.85f);

    // Wheels (black with white hubs)
    glColor3f(0.05f, 0.05f, 0.05f);
    wheel(x + w * 0.25f, y - 6, 10);
    wheel(x + w * 0.75f, y - 6, 10);
    glColor3f(1.0f, 1.0f, 1.0f);
    wheel(x + w * 0.25f, y - 6, 4);
    wheel(x + w * 0.75f, y - 6, 4);

    // Headlight (front right)
    glColor3f(0.6f, 1.0f, 0.6f);
    rect(x + w - 6, y + h * 0.3f, x + w - 1, y + h * 0.6f);

    // Taillight (rear left)
    glColor3f(1.0f, 0.2f, 0.2f);
    rect(x + 1, y + h * 0.3f, x + 6, y + h * 0.6f);
}



void cloud(float cx, float cy, float scale)
{
    glColor3f(1.0f,1.0f,1.0f);
    wheel(cx,       cy,       0.9f*scale);  // center
    wheel(cx-0.8f*scale, cy,  0.7f*scale);  // left puff
    wheel(cx+0.8f*scale, cy,  0.7f*scale);  // right puff
    wheel(cx-0.4f*scale, cy+0.6f*scale, 0.6f*scale); // upper left
    wheel(cx+0.4f*scale, cy+0.6f*scale, 0.6f*scale); // upper right
}


void layered_tree(float cx, float base)
{
    // trunk
    glColor3f(0.45f,0.25f,0.1f);
    rect(cx-8, base, cx+8, base+60);

    // foliage: overlapping circles for a rounded canopy
    glColor3f(0.12f,0.5f,0.12f);

    // bottom layer
    wheel(cx,     base+90, 40);
    wheel(cx-30,  base+85, 35);
    wheel(cx+30,  base+85, 35);

    // middle layer
    wheel(cx,     base+130, 32);
    wheel(cx-25,  base+125, 28);
    wheel(cx+25,  base+125, 28);

    // top layer
    wheel(cx,     base+165, 26);
    wheel(cx-18,  base+160, 22);
    wheel(cx+18,  base+160, 22);
}


// Add this above draw_background_buildings()
void draw_building_windows(int x1,int y1,int x2,int y2,int wx,int wy)
{
    for(int bx = x1+10; bx+wx <= x2-10; bx += wx+15)
    {
        for(int by = y1+20; by+wy <= y2-10; by += wy+20)
        {
            rect((float)bx, (float)by, (float)(bx+wx), (float)(by+wy));
        }
    }
}

void draw_background_buildings()
{
    // far back skyline (small, desaturated)
    glColor3f(0.65f,0.65f,0.7f);
    rect(50,400,200,900);   // Building A
    glColor3f(0.6f,0.6f,0.65f);
    rect(230,450,370,1000); // Building B
    glColor3f(0.55f,0.6f,0.6f);
    rect(400,420,520,920);  // Building C
    glColor3f(0.5f,0.55f,0.6f);
    rect(560,460,760,1100); // Building D
    glColor3f(0.45f,0.5f,0.55f);
    rect(820,500,880,1250); // Building E
    rect(920,520,980,1200); // Building F
    glColor3f(0.7f,0.6f,0.6f);
    rect(1900,380,2040,1100); // Building G
    glColor3f(0.6f,0.7f,0.7f);
    rect(2070,400,2210,1000); // Building H

    // background windows (aligned per building)
    glColor3f(0.85f,0.9f,0.95f);
    draw_building_windows(50,400,200,900,20,30);
    draw_building_windows(230,450,370,1000,20,30);
    draw_building_windows(400,420,520,920,20,30);
    draw_building_windows(560,460,760,1100,20,30);
    draw_building_windows(820,500,880,1250,18,28);
    draw_building_windows(920,520,980,1200,18,28);
    draw_building_windows(1900,380,2040,1100,22,32);
    draw_building_windows(2070,420,2210,1000,22,32);


}


// Simple line bird
void bird(float cx, float cy, float scale, float phase)
{
    glLineWidth(2);
    glBegin(GL_LINES);
    // left wing
    glVertex2f(cx-scale*20, cy);
    glVertex2f(cx, cy+scale*(10 + 4*sin(phase)));
    // right wing
    glVertex2f(cx, cy+scale*(10 + 4*sin(phase)));
    glVertex2f(cx+scale*20, cy);
    glEnd();
}


// Draw an analog clock time display
void drawClock(float cx, float cy, float r, int sim_seconds)
{
    // Background circle
    glColor3f(0.95f,0.95f,0.95f);
    wheel(cx, cy, r);
    // rim
    glColor3f(0.15f,0.15f,0.15f);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    for(int i=0; i<64; i++)
    {
        float a = (2*PI*i)/64.0f;
        glVertex2f(cx + cos(a)*(r+2), cy + sin(a)*(r+2));
    }
    glEnd();

    // marks for 12 hours
    for(int h=0; h<12; ++h)
    {
        float ang = PI/2.0f - h*(2*PI/12.0f);
        float x1 = cx + cos(ang)*(r-4);
        float y1 = cy + sin(ang)*(r-4);
        float x2 = cx + cos(ang)*(r-10);
        float y2 = cy + sin(ang)*(r-10);
        glBegin(GL_LINES);
        glVertex2f(x1,y1);
        glVertex2f(x2,y2);
        glEnd();
    }

    int sh = (sim_seconds / 3600) % 24;
    int sm = (sim_seconds / 60) % 60;
    int ss = sim_seconds % 60;

    // hour hand (12-hour dial)
    double hour_angle = PI/2.0 - ( (sh%12 + sm/60.0) * (2*PI/12.0) );
    double min_angle  = PI/2.0 - ( (sm + ss/60.0) * (2*PI/60.0) );

    glLineWidth(4);
    glColor3f(0.1f,0.1f,0.1f);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + cos(hour_angle)*(r*0.45), cy + sin(hour_angle)*(r*0.45));
    glEnd();

    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + cos(min_angle)*(r*0.75), cy + sin(min_angle)*(r*0.75));
    glEnd();

}

// Helper: linear interpolation
void lerp_color(float t, float r1,float g1,float b1, float r2,float g2,float b2, float *ro,float *go,float *bo)
{
    *ro = r1 + t*(r2-r1);
    *go = g1 + t*(g2-g1);
    *bo = b1 + t*(b2-b1);
}

void drawSkyAndCelestials(int sim_seconds)
{
    int dayStart = 6*3600;
    int dayEnd   = 18*3600;

    // Sky color
    float r,g,b;
    if(sim_seconds >= dayStart && sim_seconds <= dayEnd)
    {
        float t = (float)(sim_seconds - dayStart) / (dayEnd - dayStart);
        lerp_color(t, 0.18f,0.6f,0.95f, 0.4f,0.8f,1.0f, &r,&g,&b);
    }
    else
    {
        int tsec;
        if(sim_seconds < dayStart) tsec = sim_seconds + 24*3600 - dayEnd;
        else tsec = sim_seconds - dayEnd;
        float t = (float)tsec / (24*3600 - (dayEnd - dayStart));
        lerp_color(t, 0.02f,0.02f,0.08f, 0.1f,0.12f,0.22f, &r,&g,&b);
    }

    // Sky background
    glColor3f(r,g,b);
    rect(0, 400, SCENE_W, SCENE_H);

    // Parabola parameters
    float h = SCENE_W * 0.5f;       // peak at center
    float baseY = SCENE_H * 0.5f;   // baseline halfway up the scene
    float peakY = SCENE_H * 0.85f;  // peak near top
    float a = (peakY - baseY) / ((SCENE_W/2.0f)*(SCENE_W/2.0f));

    if(sim_seconds >= dayStart && sim_seconds <= dayEnd)
    {
        // Sun during daytime
        double t = (double)(sim_seconds - dayStart) / (double)(dayEnd - dayStart); // 0..1
        float sunx = t * SCENE_W; // move from x=0 to x=max
        float suny = -a * (sunx - h)*(sunx - h) + peakY;

        // Height factor for size/color
        float heightFactor = (suny - baseY) / (peakY - baseY);
        if(heightFactor < 0.0f) heightFactor = 0.0f;
        if(heightFactor > 1.0f) heightFactor = 1.0f;

        float sunSize = 40.0f + (1.0f - heightFactor) * 35.0f;

        float sr = 1.0f;
        float sg = 0.9f * heightFactor + 0.3f*(1.0f - heightFactor);
        float sb = 0.2f * heightFactor + 0.05f*(1.0f - heightFactor);

        glColor3f(sr, sg, sb);
        wheel(sunx, suny, sunSize);

        // Glow for sun only
        glowCircle(sunx, suny, sunSize*1.5f, sr, sg, sb, 0.4f*(1.0f - heightFactor));
    }
    else
    {
        // Moon during nighttime
        double t;
        if(sim_seconds > dayEnd)
            t = (double)(sim_seconds - dayEnd) / (double)(24*3600 - (dayEnd - dayStart));
        else
            t = (double)(sim_seconds + 24*3600 - dayEnd) / (double)(24*3600 - (dayEnd - dayStart));

        float moonx = t * SCENE_W;
        float moony = -a * (moonx - h)*(moonx - h) + peakY;

        glColor3f(0.92f,0.92f,0.98f);
        wheel(moonx, moony, 34.0f);

        // Crescent overlay
        glColor3f(r,g,b);
        wheel(moonx+12, moony+6, 28.0f);
        // No glow for moon
    }

    // Stars at night
    if(sim_seconds < (5*3600) || sim_seconds > (19*3600))
    {
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        glColor3f(1.0f,1.0f,1.0f);
        int stars[80][2] =
        {
            {127,1456},{2341,1523},{892,987},{1673,1211},{456,1598},
            {1921,862},{734,1534},{2198,779},{1345,1591},{289,1125},
            {1567,1303},{823,1614},{2456,941},{1089,768},{1834,1487},
            {512,1072},{2067,1556},{1423,828},{678,1405},{1956,1619},
            {345,1211},{2289,894},{1234,1577},{789,748},{1645,1189},
            {2523,1026},{923,1462},{1478,813},{2134,1298},{567,971},
            {1712,1144},{412,1519},{2378,867},{1156,792},{1889,1329},
            {645,1383},{2245,756},{1367,1504},{801,938},{1534,714},
            {2489,1471},{278,1095},{1623,822},{934,1448},{2156,884},
            {1401,1112},{723,1375},{2034,1501},{1267,859},{1845,1043},
            {489,1226},{2312,989},{1123,1515},{1678,774},{856,1407},
            {2421,1151},{1489,905},{634,1519},{1967,728},{378,1182},
            {2189,1337},{1012,793},{1756,1468},{567,949},{2278,816},
            {1334,1494},{912,771},{1601,1321},{2523,978},{745,842},
            {2045,1505},{1445,967},{823,1484},{2367,795},{1178,1229},
            {1890,754},{534,1411},{2156,942},{1267,1198},{678,876}
        };
        for(int i=0; i<80; i++)
        {
            glVertex2f(stars[i][0], stars[i][1]);
        }
        glEnd();
    }
}



// Streetlight (drawn with on/off based on boolean)
void streetlight(float x, float y, int on)
{
    // pole
    glColor3f(0.2f,0.2f,0.25f);
    rect(x-4, y-120, x+4, y);
    // arm
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(x+4, y-10);
    glVertex2f(x+40, y+10);
    glEnd();
    // lamp head
    if(on)
    {
        glColor3f(1.0f,0.9f,0.6f);
    }
    else
    {
        glColor3f(0.4f,0.4f,0.45f);
    }
    rect(x+36, y+6, x+52, y+26);
    // glow when on
    if(on)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Brighter glow during snow
        float glowAlpha = snowEnabled ? 0.45f : 0.25f;
        float glowSize = snowEnabled ? 60.0f : 40.0f;
        glColor4f(1.0f,0.95f,0.7f, glowAlpha);
        wheel(x+44, y+18, glowSize);
        glDisable(GL_BLEND);
    }
}

// Draw buildingbuilding windows but allow "night lit" flag for each window
void draw_window(float x, float y, int lit)
{
    if(lit)
    {
        // Brighter windows during snow
        if(snowEnabled)
            glColor3f(1.0f, 0.95f, 0.6f); // Brighter yellow
        else
            glColor3f(1.0f, 0.86f, 0.45f); // Normal yellow
    }
    else
    {
        glColor3f(0.12f,0.2f,0.28f);
    }
    rect(x,y,x+40,y+50);
    // divider
    glColor3f(0.06f,0.06f,0.08f);
    glBegin(GL_LINES);
    glVertex2f(x+20,y);
    glVertex2f(x+20,y+50);
    glEnd();
}

// Draw a simple letter using rectangles
void drawLetter(char letter, float x, float y, float size, int isNight) {
    float w = size * 0.6f;  // width
    float h = size;         // height
    float t = size * 0.15f; // thickness

    // Set color - white glow at night, dark during day
    if (isNight) {
        glColor3f(1.0f, 1.0f, 1.0f);
    } else {
        glColor3f(0.15f, 0.15f, 0.15f);
    }

    switch(letter) {
        case 'D':
            rect(x, y, x+t, y+h); // vertical line
            rect(x, y, x+w*0.8f, y+t); // bottom horizontal
            rect(x, y+h-t, x+w*0.8f, y+h); // top horizontal
            rect(x+w*0.8f-t, y+t, x+w*0.8f, y+h-t); // right vertical
            break;
        case 'A':
            rect(x, y, x+t, y+h); // left vertical
            rect(x+w-t, y, x+w, y+h); // right vertical
            rect(x, y+h-t, x+w, y+h); // top horizontal
            rect(x+t, y+h*0.4f, x+w-t, y+h*0.4f+t); // middle bar
            break;
        case 'F':
            rect(x, y, x+t, y+h); // vertical line
            rect(x, y+h-t, x+w, y+h); // top horizontal
            rect(x, y+h*0.5f, x+w*0.7f, y+h*0.5f+t); // middle bar
            break;
        case 'O':
            rect(x, y, x+t, y+h); // left vertical
            rect(x+w-t, y, x+w, y+h); // right vertical
            rect(x, y, x+w, y+t); // bottom horizontal
            rect(x, y+h-t, x+w, y+h); // top horizontal
            break;
        case 'I':
            rect(x+w*0.35f, y, x+w*0.65f, y+h); // vertical line
            rect(x, y, x+w, y+t); // bottom horizontal
            rect(x, y+h-t, x+w, y+h); // top horizontal
            break;
        case 'L':
            rect(x, y, x+t, y+h); // vertical line
            rect(x, y, x+w, y+t); // bottom horizontal
            break;
        case 'N':
            rect(x, y, x+t, y+h); // left vertical
            rect(x+w-t, y, x+w, y+h); // right vertical
            rect(x+t, y+h*0.3f, x+w-t, y+h*0.7f); // diagonal (approximated)
            break;
        case 'T':
            rect(x, y+h-t, x+w, y+h); // top horizontal
            rect(x+w*0.4f, y, x+w*0.6f, y+h); // vertical line
            break;
        case 'E':
            rect(x, y, x+t, y+h); // vertical line
            rect(x, y, x+w, y+t); // bottom horizontal
            rect(x, y+h-t, x+w, y+h); // top horizontal
            rect(x, y+h*0.45f, x+w*0.8f, y+h*0.45f+t); // middle bar
            break;
        case 'R':
            rect(x, y, x+t, y+h); // vertical line
            rect(x, y+h-t, x+w, y+h); // top horizontal
            rect(x+w-t, y+h*0.5f, x+w, y+h-t); // top right vertical
            rect(x, y+h*0.45f, x+w, y+h*0.45f+t); // middle bar
            rect(x+w*0.5f, y, x+w, y+h*0.45f); // diagonal leg (approximated)
            break;
        case 'U':
            rect(x, y+t, x+t, y+h); // left vertical
            rect(x+w-t, y+t, x+w, y+h); // right vertical
            rect(x, y, x+w, y+t); // bottom horizontal
            break;
        case 'S':
            rect(x, y+h-t, x+w, y+h); // top horizontal
            rect(x, y+h*0.5f-t, x+w, y+h*0.5f+t); // middle horizontal
            rect(x, y, x+w, y+t); // bottom horizontal
            rect(x, y+h*0.5f, x+t, y+h); // top left vertical
            rect(x+w-t, y, x+w, y+h*0.5f); // bottom right vertical
            break;
        case 'V':
            rect(x, y+h*0.3f, x+t, y+h); // left vertical (upper)
            rect(x+w-t, y+h*0.3f, x+w, y+h); // right vertical (upper)
            rect(x+w*0.4f, y, x+w*0.6f, y+h*0.4f); // bottom center
            break;
        case 'Y':
            rect(x, y+h*0.5f, x+t, y+h); // top left
            rect(x+w-t, y+h*0.5f, x+w, y+h); // top right
            rect(x+w*0.4f, y, x+w*0.6f, y+h*0.6f); // vertical center
            break;
    }

    // No separate glow - letters themselves will be white at night
}

// Draw billboard with "DAFFODIL INTERNATIONAL UNIVERSITY"
void drawBillboard(float x, float y, float width, float height, int isNight) {
    // Billboard background - dark at night, light gray during day
    if (isNight) {
        glColor3f(0.15f, 0.15f, 0.2f);
    } else {
        glColor3f(0.85f, 0.85f, 0.9f);
    }
    rect(x, y, x + width, y + height);

    // Border
    glColor3f(0.3f, 0.3f, 0.35f);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    // Text to display
    const char* text = "DAFFODIL INTERNATIONAL UNIVERSITY";

    // Calculate text width using GLUT font
    int textWidth = 0;
    for(const char *c = text; *c != '\0'; c++) {
        textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);
    }

    // GLUT_BITMAP_HELVETICA_18 has a height of approximately 18 pixels
    // For proper centering, we need to account for the font baseline
    float fontHeight = 18.0f;

    // Center the text both horizontally and vertically
    float textX = x + (width - textWidth) / 2.0f - 100.0f;
    float textY = y + (height / 2.0f) - 10.0f; // Vertically center with baseline adjustment

    // Set text color - white at night, dark during day
    if (isNight) {
        glColor3f(1.0f, 1.0f, 1.0f);
    } else {
        glColor3f(0.15f, 0.15f, 0.15f);
    }

    // Draw the text using GLUT bitmap font
    glRasterPos2f(textX, textY);
    for(const char *c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

// Draw kids and ball (simple stylized figures)
void draw_child(float cx, float cy, float scale, float phase)
{
    // body (use whatever color is set before calling)
    rect(cx-6*scale, cy, cx+6*scale, cy+18*scale);

    // head - brighter, more saturated skin tone
    glColor3f(1.0f,0.75f,0.55f);
    wheel(cx, cy+26*scale, 8*scale);

    // legs (animated using phase)
    float leg_offset = sin(phase)*6.0f*scale;
    glLineWidth(4);
    glColor3f(0.1f,0.1f,0.12f);
    glBegin(GL_LINES);
    glVertex2f(cx-4*scale, cy);
    glVertex2f(cx-8*scale+leg_offset, cy-14*scale);
    glVertex2f(cx+4*scale, cy);
    glVertex2f(cx+8*scale-leg_offset, cy-14*scale);
    glEnd();

    // arms - darker, more visible color
    glColor3f(0.05f,0.05f,0.08f);
    glBegin(GL_LINES);
    glVertex2f(cx-6*scale, cy+12*scale);
    glVertex2f(cx-12*scale, cy+8*scale + 3.0f*sin(phase));
    glVertex2f(cx+6*scale, cy+12*scale);
    glVertex2f(cx+12*scale, cy+8*scale - 3.0f*sin(phase));
    glEnd();
}


// --- Global animation/time helpers ---
double get_simulated_seconds_of_day()
{
    // real current time-of-day seconds (0..86399)
    time_t now = time(NULL);
    struct tm *tmnow = localtime(&now);
    int real_seconds = tmnow->tm_hour*3600 + tmnow->tm_min*60 + tmnow->tm_sec;
    // We'll derive a monotonic simulated second value from real_seconds plus fractional milliseconds
    // For smoother animation get GLUT elapsed time
    int real_ms = glutGet(GLUT_ELAPSED_TIME); // ms since program start
    double extra_real = (real_ms % 1000) / 1000.0; // fraction of current second
    double real_time_with_frac = real_seconds + extra_real;
    double sim_seconds = fmod(real_time_with_frac * SIM_SPEED, 24.0*3600.0);
    if(sim_seconds < 0) sim_seconds += 24.0*3600.0;
    return sim_seconds;
}

// Timer callback to refresh
void timer_func(int v)
{
    // advance smoothly: TIMER_MS milliseconds * SIM_SPEED
    double deltaTime = TIMER_MS / 1000.0;
    sim_seconds_d = fmod(sim_seconds_d + deltaTime * SIM_SPEED, 24.0*3600.0);

    // Update snowflakes
    updateSnowflakes(deltaTime);

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, timer_func, 0);
}

void specialKeys(int key, int x, int y)
{
    float step = 10.0f; // movement step size
    switch (key)
    {
    case GLUT_KEY_LEFT:
        ballx -= step;
        break;
    case GLUT_KEY_RIGHT:
        ballx += step;
        break;
    case GLUT_KEY_UP:
        bally += step;
        break;
    case GLUT_KEY_DOWN:
        bally -= step;
        break;
    }
    glutPostRedisplay(); // redraw after movement
}


void drawFerrisWheel(double sim_seconds)
{
    float cx = SCENE_W - 150;   // center near right edge
    float cy = 500;             // vertical center
    float radius = 200.0f;      // wheel radius

    // Draw full wheel rim
    glColor3f(0.7f,0.7f,0.75f);
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    for(int i=0; i<360; i+=5)
    {
        float a = i * PI / 180.0f;
        glVertex2f(cx + cos(a)*radius, cy + sin(a)*radius);
    }
    glEnd();

    // Central hub
    glColor3f(0.5f,0.5f,0.6f);
    wheel(cx, cy, 12.0f);

    // Spokes
    int numSpokes = 12;
    for(int i=0; i<numSpokes; i++)
    {
        float a = i * 2*PI / numSpokes;
        float x = cx + cos(a)*radius;
        float y = cy + sin(a)*radius;
        glBegin(GL_LINES);
        glVertex2f(cx, cy);
        glVertex2f(x, y);
        glEnd();
    }

    // Animate red gondolas
    int numCabins = 12;
    double rotationSpeed = 0.2;
    double angleOffset = fmod(sim_seconds * rotationSpeed, 2*PI);

    for(int i=0; i<numCabins; i++)
    {
        double a = angleOffset + i*(2*PI/numCabins);
        float x = cx + cos(a)*radius;
        float y = cy + sin(a)*radius;

        glColor3f(0.9f, 0.2f, 0.2f); // red gondola
        rect(x-10, y-10, x+10, y+10);
    }

    // Base stand
    glColor3f(0.3f,0.3f,0.35f);
    glLineWidth(6);
    glBegin(GL_LINES);
    glVertex2f(cx-40, cy-200);
    glVertex2f(cx, cy-20);
    glVertex2f(cx+40, cy-200);
    glVertex2f(cx, cy-20);
    glEnd();
}



// --- Enhanced realistic car ---
void drawRealisticCar(float x, float y, float w, float h, float r, float g, float b) {
    // Main body (lower section)
    glColor3f(r, g, b);
    rect(x, y, x + w, y + h * 0.5f);

    // Cabin/roof (upper section) - narrower and set back
    float cabinStartX = x + w * 0.25f;
    float cabinEndX = x + w * 0.75f;
    float cabinY = y + h * 0.5f;
    float cabinH = h * 0.4f;

    // Darken cabin slightly for depth
    glColor3f(r * 0.9f, g * 0.9f, b * 0.9f);
    rect(cabinStartX, cabinY, cabinEndX, cabinY + cabinH);

    // Front windshield - square window
    glColor3f(0.4f, 0.5f, 0.6f); // slightly blue tint
    rect(cabinStartX + w * 0.05f, cabinY + cabinH * 0.15f,
         cabinStartX + w * 0.22f, cabinY + cabinH * 0.85f);

    // Rear window - square window
    glColor3f(0.35f, 0.45f, 0.55f);
    rect(cabinEndX - w * 0.22f, cabinY + cabinH * 0.15f,
         cabinEndX - w * 0.05f, cabinY + cabinH * 0.85f);

    // Side windows (dark tint) - square windows
    glColor3f(0.15f, 0.2f, 0.25f);
    rect(cabinStartX + w * 0.28f, cabinY + cabinH * 0.2f,
         cabinEndX - w * 0.28f, cabinY + cabinH * 0.85f);

    // Front grille
    glColor3f(0.1f, 0.1f, 0.1f);
    rect(x + w - 8, y + h * 0.15f, x + w - 2, y + h * 0.45f);

    // Grille details (horizontal lines)
    glColor3f(0.3f, 0.3f, 0.3f);
    glLineWidth(1);
    glBegin(GL_LINES);
    for(int i = 0; i < 3; i++) {
        float gy = y + h * 0.2f + i * h * 0.08f;
        glVertex2f(x + w - 7, gy);
        glVertex2f(x + w - 3, gy);
    }
    glEnd();

    // Wheels with more detail
    glColor3f(0.08f, 0.08f, 0.08f); // tire
    wheel(x + w * 0.22f, y - 4, 12);
    wheel(x + w * 0.78f, y - 4, 12);

    glColor3f(0.25f, 0.25f, 0.25f); // rim outer
    wheel(x + w * 0.22f, y - 4, 8);
    wheel(x + w * 0.78f, y - 4, 8);

    glColor3f(0.7f, 0.7f, 0.75f); // rim inner (metallic)
    wheel(x + w * 0.22f, y - 4, 5);
    wheel(x + w * 0.78f, y - 4, 5);

    glColor3f(0.3f, 0.3f, 0.35f); // hub center
    wheel(x + w * 0.22f, y - 4, 2);
    wheel(x + w * 0.78f, y - 4, 2);

    // Headlights (dual lights with glow)
    glColor3f(0.95f, 0.95f, 0.7f);
    wheel(x + w - 5, y + h * 0.35f, 3);
    wheel(x + w - 5, y + h * 0.2f, 3);

    // Subtle glow for headlights
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.95f, 0.7f, 0.3f);
    wheel(x + w - 5, y + h * 0.35f, 6);
    wheel(x + w - 5, y + h * 0.2f, 6);
    glDisable(GL_BLEND);

    // Taillights (dual)
    glColor3f(0.9f, 0.1f, 0.1f);
    wheel(x + 3, y + h * 0.35f, 2.5f);
    wheel(x + 3, y + h * 0.2f, 2.5f);

    // Door handle/line detail
    glColor3f(r * 0.7f, g * 0.7f, b * 0.7f);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(x + w * 0.35f, y + h * 0.3f);
    glVertex2f(x + w * 0.65f, y + h * 0.3f);
    glEnd();

    // Side mirror (small rectangle)
    glColor3f(r * 0.8f, g * 0.8f, b * 0.8f);
    rect(cabinStartX - 6, cabinY + cabinH * 0.5f,
         cabinStartX - 2, cabinY + cabinH * 0.7f);

    // Bumper detail
    glColor3f(0.2f, 0.2f, 0.2f);
    rect(x + w - 3, y, x + w - 1, y + h * 0.12f);
    rect(x + 1, y, x + 3, y + h * 0.12f);
}

// --- Enhanced realistic bus ---
void drawRealisticBus(float x, float y, float w, float h) {
    float busY_top = y + h;

    // Main bus body (orange/yellow)
    glColor3f(0.95f, 0.65f, 0.15f);
    rect(x, y, x + w, busY_top);

    // Darker stripe along bottom
    glColor3f(0.75f, 0.45f, 0.1f);
    rect(x, y, x + w, y + h * 0.25f);

    // Top stripe detail
    glColor3f(0.85f, 0.55f, 0.12f);
    rect(x, busY_top - 6, x + w, busY_top - 3);

    // Front section (slightly darker for depth)
    glColor3f(0.85f, 0.55f, 0.12f);
    rect(x + w - 15, y, x + w, busY_top);

    // Multiple windows (5 windows for a bus look)
    glColor3f(0.5f, 0.75f, 0.9f); // Light blue tinted windows
    float windowY1 = y + h * 0.45f;
    float windowY2 = busY_top - 5;
    float windowSpacing = 8;
    float windowWidth = 28;

    // Window 1
    rect(x + 12, windowY1, x + 12 + windowWidth, windowY2);
    // Window 2
    rect(x + 12 + windowWidth + windowSpacing, windowY1, x + 12 + 2*windowWidth + windowSpacing, windowY2);
    // Window 3
    rect(x + 12 + 2*(windowWidth + windowSpacing), windowY1, x + 12 + 3*windowWidth + 2*windowSpacing, windowY2);
    // Window 4
    rect(x + 12 + 3*(windowWidth + windowSpacing), windowY1, x + 12 + 4*windowWidth + 3*windowSpacing, windowY2);
    // Window 5 (smaller, rear)
    rect(x + 12 + 4*(windowWidth + windowSpacing), windowY1, x + 12 + 4*windowWidth + 3*windowSpacing + 20, windowY2);

    // Windshield (front window) - slightly different tint
    glColor3f(0.45f, 0.65f, 0.8f);
    rect(x + w - 12, windowY1, x + w - 3, windowY2);

    // Front grille/bumper
    glColor3f(0.15f, 0.15f, 0.15f);
    rect(x + w - 3, y + h * 0.15f, x + w - 1, y + h * 0.35f);

    // Headlights (dual)
    glColor3f(0.95f, 0.95f, 0.7f);
    wheel(x + w - 5, y + h * 0.5f, 3);
    wheel(x + w - 5, y + h * 0.3f, 3);

    // Headlight glow
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.95f, 0.7f, 0.3f);
    wheel(x + w - 5, y + h * 0.5f, 6);
    wheel(x + w - 5, y + h * 0.3f, 6);
    glDisable(GL_BLEND);

    // Taillights (dual, larger)
    glColor3f(0.95f, 0.15f, 0.15f);
    wheel(x + 4, y + h * 0.5f, 3);
    wheel(x + 4, y + h * 0.3f, 3);

    // Door outline (front door)
    glColor3f(0.6f, 0.35f, 0.08f);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + w - 30, y + h * 0.08f);
    glVertex2f(x + w - 18, y + h * 0.08f);
    glVertex2f(x + w - 18, y + h * 0.42f);
    glVertex2f(x + w - 30, y + h * 0.42f);
    glEnd();

    // Side mirror
    glColor3f(0.25f, 0.25f, 0.25f);
    rect(x + w - 15, busY_top - 8, x + w - 12, busY_top - 2);

    // Wheels with details
    float wheelY = y - 6;

    // Front wheel (larger outer tire)
    glColor3f(0.08f, 0.08f, 0.08f);
    wheel(x + w * 0.75f, wheelY, 14);

    // Rear wheel
    wheel(x + w * 0.25f, wheelY, 14);

    // Wheel rims (outer)
    glColor3f(0.3f, 0.3f, 0.3f);
    wheel(x + w * 0.75f, wheelY, 10);
    wheel(x + w * 0.25f, wheelY, 10);

    // Wheel rims (inner metallic)
    glColor3f(0.6f, 0.6f, 0.65f);
    wheel(x + w * 0.75f, wheelY, 6);
    wheel(x + w * 0.25f, wheelY, 6);

    // Hub centers
    glColor3f(0.25f, 0.25f, 0.25f);
    wheel(x + w * 0.75f, wheelY, 3);
    wheel(x + w * 0.25f, wheelY, 3);

    // "SCHOOL BUS" or route number sign (optional detail)
    glColor3f(0.1f, 0.1f, 0.1f);
    rect(x + w * 0.42f, busY_top + 2, x + w * 0.58f, busY_top + 8);

    // Destination sign text effect (small rectangles)
    glColor3f(1.0f, 0.5f, 0.0f);
    rect(x + w * 0.44f, busY_top + 3.5f, x + w * 0.47f, busY_top + 6.5f);
    rect(x + w * 0.48f, busY_top + 3.5f, x + w * 0.51f, busY_top + 6.5f);
    rect(x + w * 0.52f, busY_top + 3.5f, x + w * 0.55f, busY_top + 6.5f);
}



// Draw the whole scene
void Draw()
{
    glClear(GL_COLOR_BUFFER_BIT);

    int sim_seconds = (int)floor(sim_seconds_d);
    int sim_hour = (sim_seconds / 3600) % 24;


    glColor3f(0.49f,0.67f,0.28f);
    rect(0,270,SCENE_W,400);  // place between grass (0–270) and sky (400–SCENE_H)


    // Sky + sun/moon
    drawSkyAndCelestials(sim_seconds);

    // Apply dimming overlay when snow is active (excluding lights)
    if (snowEnabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Dark semi-transparent overlay
        float dimAlpha = 0.4f; // 40% darkness
        glColor4f(0.1f, 0.1f, 0.15f, dimAlpha);
        rect(0, 0, SCENE_W, SCENE_H);
        glDisable(GL_BLEND);
    }

    // Grass field preserved (foreground)
    glColor3f(0.36f,0.53f,0.24f);
    rect(0,0,SCENE_W,270);

    // mid grass layers
    glColor3f(0.49f,0.67f,0.28f);
    glBegin(GL_POLYGON);
    glVertex2f(SCENE_W, 47);
    glVertex2f(SCENE_W, 263);
    glVertex2f(845, 280);
    glVertex2f(10, 232);
    glVertex2f(5, 227);
    glVertex2f(10, 222);
    glVertex2f(590, 165);
    glVertex2f(595, 160);
    glVertex2f(590, 155);
    glVertex2f(383, 130);
    glVertex2f(378, 125);
    glVertex2f(378, 122);
    glVertex2f(383, 120);
    glVertex2f(SCENE_W, 47);
    glEnd();

    // road
    glColor3f(0.12f,0.12f,0.12f);
    rect(0,170,SCENE_W,250);
    // sidewalks
    glColor3f(0.7f,0.7f,0.7f);
    rect(0,250,SCENE_W,265);
    rect(0,150,SCENE_W,170);

    // lane markings
    glColor3f(1.0f,1.0f,0.05f);
    glLineWidth(6);
    glBegin(GL_LINES);
    for(int i=50; i<SCENE_W-20; i+=120)
    {
        glVertex2f(i,210);
        glVertex2f(i+60,210);
    }
    glEnd();




    // background buildings
    draw_background_buildings();

    // Main building front (light gray)
    glColor3f(0.75f, 0.75f, 0.75f);
    rect(400,280,1800,900);
    glColor3f(0.7f,0.7f,0.7f);
    rect(200,300,450,800);
    rect(1750,300,2000,800);
    glColor3f(0.8f,0.8f,0.8f);
    rect(900,280,1300,600);


    // Building floors lines
    glColor3f(0.6f,0.6f,0.6f);
    glLineWidth(2);
    glBegin(GL_LINES);
    for(int i = 350; i <= 850; i += 70)
    {
        glVertex2f(400,i);
        glVertex2f(1800,i);
    }
    for(int i = 370; i <= 750; i += 70)
    {
        glVertex2f(200,i);
        glVertex2f(450,i);
    }
    for(int i = 370; i <= 750; i += 70)
    {
        glVertex2f(1750,i);
        glVertex2f(2000,i);
    }
    glEnd();

    // Decide night lights: on when before 6:00 or after 18:00
    int isNight = (sim_hour < 6 || sim_hour >= 18) ? 1 : 0;
    // Soft evening dusk factor to tint colors (0..1)
    float duskFactor = 0.0f;
    if(sim_seconds >= (5*3600) && sim_seconds < (6*3600)) // dawn transition
        duskFactor = 1.0f - (float)(sim_seconds - 5*3600) / 3600.0f;
    else if(sim_seconds >= (18*3600) && sim_seconds < (19*3600)) // dusk transition
        duskFactor = (float)(sim_seconds - 18*3600) / 3600.0f;
    else
        duskFactor = isNight ? 1.0f : 0.0f;

    // Billboard on the front of main building (centered)
    // Main building: X 400-1800 (width 1400), center at X 1100
    // Billboard width 1000, so start at X 600 (1100 - 500)
    drawBillboard(650, 800, 950, 90, isNight);

    // Windows - Main building (center block)
    for(int floor = 0; floor < 8; floor++)
    {
        int y = 320 + floor * 70;
        if(y + 50 > 850) continue; // prevent overflow
        for(int window = 0; window < 20; window++)
        {
            int x = 450 + window * 65;
            if(x + 40 > 1750) continue; // prevent overflow
            int lit = 0;
            if(isNight)
            {
                if(((window+floor*3) % 3) != 0) lit = 1;
            }
            else
            {
                lit = 0; // force all windows off during day
            }

            draw_window(x, y, lit);
        }
    }

// Windows - Left wing
    for(int floor = 0; floor < 7; floor++)
    {
        int y = 340 + floor * 70;
        if(y + 50 > 800) continue;
        for(int window = 0; window < 3; window++)
        {
            int x = 220 + window * 70;
            if(x + 40 > 450) continue;
            int lit = isNight ? ((window+floor)%2==0) : 0;
            draw_window(x, y, lit);
        }
    }

// Windows - Right wing
    for(int floor = 0; floor < 7; floor++)
    {
        int y = 340 + floor * 70;
        if(y + 50 > 800) continue;
        for(int window = 0; window < 3; window++)
        {
            int x = 1770 + window * 70;
            if(x + 40 > 2000) continue;
            int lit = isNight ? ((window+floor)%2==1) : 0;
            draw_window(x, y, lit);
        }
    }


    // entrance doors
    glColor3f(0.35f,0.25f,0.15f);
    rect(1020,280,1180,380);
    glColor3f(0.6f,0.85f,1.0f);
    rect(1030,300,1070,360);
    rect(1130,300,1170,360);


    // Draw ground snow accumulation first (under snowflakes)
    drawGroundSnow();

    drawFerrisWheel(sim_seconds_d);

// ---------------- Stores on the far right side ----------------
    glColor3f(0.7f,0.7f,0.75f); // base wall color
    rect(2000,250,2150,400); // Store 1
    rect(2170,250,2320,400); // Store 2
    rect(2340,250,2490,400); // Store 3

// Store doors
    glColor3f(0.35f,0.25f,0.15f);
    rect(2040,250,2080,330); // door store 1
    rect(2210,250,2250,330); // door store 2
    rect(2380,250,2420,330); // door store 3

// Storefront windows
    glColor3f(0.6f,0.85f,1.0f);
    rect(2090,300,2130,360);
    rect(2260,300,2300,360);
    rect(2430,300,2470,360);

// --- Right side enrichments ---
// Customer near Cafe Blue
    glColor3f(0.9f,0.6f,0.6f);
    rect(2020, 260, 2030, 280);
    glColor3f(0.98f,0.86f,0.7f);
    wheel(2025, 288, 6);
    glColor3f(0.6f,0.85f,1.0f);
    rect(2030, 270, 2035, 275);

// Delivery person near Tech Shop
    glColor3f(0.6f,0.6f,0.9f);
    rect(2450, 260, 2460, 280);
    glColor3f(0.98f,0.86f,0.7f);
    wheel(2455, 288, 6);
    glColor3f(0.8f,0.5f,0.2f);
    rect(2460, 265, 2470, 275);

// Bicycle near Book Hub
    glColor3f(0.2f,0.2f,0.2f);
    glBegin(GL_LINES);
    glVertex2f(2250, 255);
    glVertex2f(2260, 265);
    glVertex2f(2260, 265);
    glVertex2f(2270, 255);
    glVertex2f(2250, 255);
    glVertex2f(2270, 255);
    glEnd();
    wheel(2250, 255, 5);
    wheel(2270, 255, 5);


// Store signs with names + glow at night
    if(isNight)
    {
        // dark base sign
        glColor3f(0.05f,0.05f,0.2f);
        rect(2000,370,2150,395); // sign store 1
        rect(2170,370,2320,395); // sign store 2
        rect(2340,370,2490,395); // sign store 3

        // neon glow overlay
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.2f,0.8f,1.0f,0.35f); // cyan glow
        rect(1990,365,2160,400); // glow store 1
        rect(2160,365,2330,400); // glow store 2
        rect(2330,365,2500,400); // glow store 3
        glDisable(GL_BLEND);

        // Billboard text glowing
        glColor3f(0.8f,1.0f,1.0f); // bright text
        glRasterPos2f(2020,380);
        const char *name1 = "Cafe Blue";
        for(const char *c=name1; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

        glRasterPos2f(2190,380);
        const char *name2 = "Book Hub";
        for(const char *c=name2; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

        glRasterPos2f(2360,380);
        const char *name3 = "Tech Shop";
        for(const char *c=name3; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    else
    {
        // daytime signs
        glColor3f(0.2f,0.4f,0.7f);
        rect(2000,370,2150,395); // sign store 1
        rect(2170,370,2320,395); // sign store 2
        rect(2340,370,2490,395); // sign store 3

        // Billboard text normal
        glColor3f(1.0f,1.0f,1.0f);
        glRasterPos2f(2020,380);
        const char *name1 = "Cafe Blue";
        for(const char *c=name1; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

        glRasterPos2f(2190,380);
        const char *name2 = "Book Hub";
        for(const char *c=name2; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

        glRasterPos2f(2360,380);
        const char *name3 = "Tech Shop";
        for(const char *c=name3; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }



    // Trees flanking scene
    layered_tree(100,250);
    layered_tree(2100,250);
    layered_tree(300,230);
    layered_tree(1900,230);




    // Walkway/path in front of building
    glColor3f(0.5f,0.5f,0.5f);
    rect(800,250,1400,280);

    // Sidewalk planters
    glColor3f(0.2f,0.6f,0.2f);
    rect(750,250,780,280);
    rect(1320,250,1350,280);

    // Roadside streetlights (modern style) along the sidewalk
    for(int sx=100; sx<SCENE_W-20; sx+=240)
    {
        // streetlights on when night or shortly during dusk
        int on = (isNight || duskFactor > 0.4f) ? 1 : 0;
        streetlight(sx, 265, on);
    }

// ---------------- Animated Vehicles on the Road ----------------
// Use existing simulated time value: sim_seconds_d

// Red car
float redX = fmod(sim_seconds_d * 0.06f, SCENE_W + 140.0f) - 140.0f;
drawRealisticCar(redX, 210.0f, 140.0f, 36.0f, 0.85f, 0.1f, 0.1f);

// Green car (replace the manual drawing with):
float greenSpeed = 0.08f;
float greenW = 130.0f;
float greenX = fmod(sim_seconds_d * greenSpeed + 300.0f, SCENE_W + greenW) - greenW;
drawRealisticCar(greenX, 210.0f, 130.0f, 33.0f, 0.12f, 0.62f, 0.12f);

// Blue Police car (replace the manual drawing, keep flashing lights after):
float blueSpeed = 0.1f;
float blueW = 170.0f;
float blueX = fmod(sim_seconds_d * blueSpeed + 600.0f, SCENE_W + blueW) - blueW;
drawRealisticCar(blueX, 210.0f, 170.0f, 42.0f, 0.12f, 0.18f, 0.82f);

// Then add police lights (keep existing light bar code after this)
float blueY_bot = 210.0f;
float blueY_top = blueY_bot + 42.0f;
float lightY = blueY_top + 6;
int flash = ((int)sim_seconds / 1) % 2;

// Red light (left side)
    if(flash)
    {
        glColor3f(1.0f, 0.2f, 0.2f); // bright red
    }
    else
    {
        glColor3f(0.4f, 0.0f, 0.0f); // dim red
    }
    rect(blueX + 50, lightY, blueX + 70, lightY + 8);

// Blue light (right side)
    if(!flash)
    {
        glColor3f(0.2f, 0.2f, 1.0f); // bright blue
    }
    else
    {
        glColor3f(0.0f, 0.0f, 0.4f); // dim blue
    }
    rect(blueX + 90, lightY, blueX + 110, lightY + 8);

    glColor3f(1.0f,0.95f,0.6f);
    rect(blueX + blueW - 6, blueY_bot + 7, blueX + blueW - 1, blueY_bot + 15);
    glColor3f(1.0f,0.18f,0.18f);
    rect(blueX + 1, blueY_bot + 7, blueX + 6, blueY_bot + 15);
    glColor3f(0.05f,0.05f,0.05f);
    wheel(blueX + 48, blueY_bot - 6, 12);
    wheel(blueX + 128, blueY_bot - 6, 12);

// Bus (very slow, larger)
    float busSpeed = 0.05f;
    float busW = 180.0f;
    float busX = fmod(sim_seconds_d * busSpeed + 900.0f, SCENE_W + busW) - busW;
    float busY_bot = 212.0f;
    float busH = 44.0f;
    drawRealisticBus(busX, busY_bot, busW, busH);


    // Clouds (day) and faint ones at dusk/night (lighter)
    // Clouds (day) and faint ones at dusk/night (lighter)
// Clouds drifting across the sky
    if(!isNight || duskFactor > 0.1f)
    {
        double t = sim_seconds_d; // simulated time
        float speed = 0.08f;      // pixels per simulated second

        // cloud positions (wrap around scene width) with random offsets
        float cx1 = fmod(t*speed       + cloudOffsetsX[0], SCENE_W+300) - 150;
        float cx2 = fmod(t*speed*0.8   + cloudOffsetsX[1], SCENE_W+300) - 150;
        float cx3 = fmod(t*speed*0.7   + cloudOffsetsX[2], SCENE_W+300) - 150;
        float cx4 = fmod(t*speed*0.9   + cloudOffsetsX[3], SCENE_W+300) - 150;
        float cx5 = fmod(t*speed*0.6   + cloudOffsetsX[4], SCENE_W+300) - 150;
        float cx6 = fmod(t*speed*1.1   + cloudOffsetsX[5], SCENE_W+300) - 150;
        float cx7 = fmod(t*speed*0.5   + cloudOffsetsX[6], SCENE_W+300) - 150;
        float cx8 = fmod(t*speed*1.2   + cloudOffsetsX[7], SCENE_W+300) - 150;

        cloud(cx1, cloudOffsetsY[0], cloudScales[0]);
        cloud(cx2, cloudOffsetsY[1], cloudScales[1]);
        cloud(cx3, cloudOffsetsY[2], cloudScales[2]);
        cloud(cx4, cloudOffsetsY[3], cloudScales[3]);
        cloud(cx5, cloudOffsetsY[4], cloudScales[4]);
        cloud(cx6, cloudOffsetsY[5], cloudScales[5]);
        cloud(cx7, cloudOffsetsY[6], cloudScales[6]);
        cloud(cx8, cloudOffsetsY[7], cloudScales[7]);
    }






    // Birds flying when day
    if(!isNight)
    {
        double t = sim_seconds_d;
        float flySpeed = 0.5f;   // slower than before (was 60.0f)
        float birdX = fmod(t * flySpeed, SCENE_W + 200) - 100;
        float birdY = 1300.0f + 40.0f*sin(t*0.5);

        glColor3f(0.05f,0.05f,0.05f);
        for(int i=0; i<5; i++)
        {
            float offset = i*120.0f;
            float phase = t*4.0f + i;
            bird(birdX - offset, birdY + (i%2)*30, 1.0f, phase);
        }
    }



    // Benches
    glColor3f(0.3f,0.2f,0.15f);
    rect(870,260,930,275);
    rect(1070,260,1130,275);

// ---------------- Children playing football ----------------
// 10 spaced-out children with wider horizontal gaps
    float kx[10] = {1300, 1360, 1420, 1480, 1540, 1600, 1660, 1720, 1780, 1840};
    float ky[10] = {80, 90, 75, 85, 95, 80, 90, 75, 85, 95};

// jersey colors: first 5 red, last 5 blue
    for(int i=0; i<10; i++)
    {
        if(i%2==1)
        {
            glColor3f(0.9f, 0.1f, 0.1f); // red team
        }
        else
        {
            glColor3f(0.1f, 0.1f, 0.9f); // blue team
        }
        draw_child(kx[i], ky[i], 1.0f, 0.0f);
    }



// ball drawing (uses global ballx, bally)
    glColor3f(0.95f,0.95f,0.95f);
    wheel(ballx, bally, 8.0f);
    glColor3f(0.0f,0.0f,0.0f);
    wheel(ballx+4, bally+2, 2.0f);
    wheel(ballx-3, bally-2, 1.6f);

// ---------------- Goalposts (spaced apart) ----------------
    // Draw black borders first
    glColor3f(0.0f,0.0f,0.0f);
    glLineWidth(6);
    glBegin(GL_LINES);

// Left goalpost borders
    glVertex2f(1150, 60);
    glVertex2f(1150, 130); // left vertical
    glVertex2f(1230, 60);
    glVertex2f(1230, 130); // right vertical
    glVertex2f(1150, 130);
    glVertex2f(1230, 130); // top bar

// Right goalpost borders
    glVertex2f(1850, 60);
    glVertex2f(1850, 130); // left vertical
    glVertex2f(1930, 60);
    glVertex2f(1930, 130); // right vertical
    glVertex2f(1850, 130);
    glVertex2f(1930, 130); // top bar

    glEnd();

    // Draw white/gray goalposts on top
    glColor3f(0.85f,0.85f,0.85f);
    glLineWidth(4);
    glBegin(GL_LINES);

// Left goalpost (left side of field)
    glVertex2f(1150, 60);
    glVertex2f(1150, 130); // left vertical
    glVertex2f(1230, 60);
    glVertex2f(1230, 130); // right vertical
    glVertex2f(1150, 130);
    glVertex2f(1230, 130); // top bar

// Right goalpost (right side of field)
    glVertex2f(1850, 60);
    glVertex2f(1850, 130); // left vertical
    glVertex2f(1930, 60);
    glVertex2f(1930, 130); // right vertical
    glVertex2f(1850, 130);
    glVertex2f(1930, 130); // top bar

    glEnd();






    // ---------------- Snow System ----------------
    // Draw ground snow accumulation
    drawGroundSnow();

    // Draw falling snowflakes on top
    drawSnowflakes();



 // ---------------- Clock (bottom-left) ----------------
    float clock_cx = 80.0f, clock_cy = 80.0f, clock_r = 48.0f;
    drawClock(clock_cx, clock_cy, clock_r, sim_seconds);


    glutSwapBuffers();
}

// Initialization
void init(void)
{
    glClearColor(0.0,0.0,0.0,0.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCENE_W, 0.0, SCENE_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

    // Seed random and assign cloud offsets
    srand(time(NULL));
    for(int i=0; i<8; i++)
    {
        cloudOffsetsX[i] = rand() % (SCENE_W+300);   // random horizontal offset
        cloudOffsetsY[i] = 1200 + rand() % 400;      // random vertical position (between 1200–1600)
        cloudScales[i]   = 30 + rand() % 40;         // random size (30–70)
    }
}


// Keyboard handler for snow control
void keyboard(unsigned char key, int x, int y)
{
    switch(key) {
        case 's':
        case 'S':
            if (!snowEnabled) {
                snowEnabled = true;
                snowStartTime = sim_seconds_d;
                groundSnowLevel = 0.0f;
                initSnowflakes();
                printf("Snow started! Current sim time: %.1f seconds\n", sim_seconds_d);
            }
            break;

        case 'c':
        case 'C':
            snowEnabled = false;
            groundSnowLevel = 0.0f;
            printf("Snow cleared! Back to normal.\n");
            break;

        case 27: // ESC key
            exit(0);
            break;
    }
    glutPostRedisplay();
}

// main
int main(int argc,char **argv)
{
    glutInit(&argc,argv);
    glutInitDisplayMode ( GLUT_RGB | GLUT_DOUBLE );
    glutInitWindowPosition(50,50);
    glutInitWindowSize(900,600);
    glutCreateWindow("Final Project_Daffodil International University");
    init();
    glutDisplayFunc(Draw);
    glutSpecialFunc(specialKeys);
    glutKeyboardFunc(keyboard); // Add keyboard handler
    glutTimerFunc(TIMER_MS, timer_func, 0);
    glutMainLoop();
    return 0;
}
