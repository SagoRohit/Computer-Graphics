#include <GL/glut.h>
#include <ctime>
#include <cmath>

#define PI 3.14159265358979323846

// Radius of the clock
float clockRadius = 0.8f;

// Convert degrees to radians
float degToRad(float deg) {
    return deg * PI / 180.0f;
}

// Function to draw the clock circle and tick marks
void drawClockFace() {
    // Draw circle
    glColor3f(0.8f, 0.8f, 0.8f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 360; i++) {
        float theta = degToRad(i);
        glVertex2f(clockRadius * cos(theta), clockRadius * sin(theta));
    }
    glEnd();

    // Draw tick marks
    for (int i = 0; i < 60; ++i) {
        float angle = degToRad(i * 6);  // 360/60 = 6
        float x1 = (i % 5 == 0) ? clockRadius * 0.9f : clockRadius * 0.95f;
        float x2 = clockRadius;

        float x_start = x1 * cos(angle);
        float y_start = x1 * sin(angle);
        float x_end = x2 * cos(angle);
        float y_end = x2 * sin(angle);

        glLineWidth((i % 5 == 0) ? 2.0f : 1.0f);
        glBegin(GL_LINES);
        glVertex2f(x_start, y_start);
        glVertex2f(x_end, y_end);
        glEnd();
    }
}

// Function to draw clock hands
void drawHand(float length, float width, float angleDeg, float r, float g, float b) {
    glPushMatrix();
    glRotatef(angleDeg, 0.0f, 0.0f, -1.0f); // Rotate clockwise
    glColor3f(r, g, b);
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, length);
    glEnd();
    glPopMatrix();
}

// Display callback
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    // Get system time
    std::time_t now = std::time(0);
    std::tm *ltm = std::localtime(&now);

    // Get high-resolution seconds
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    float sec = ts.tv_sec % 60 + ts.tv_nsec / 1e9;
    float min = ltm->tm_min + sec / 60.0f;
    float hour = (ltm->tm_hour % 12) + min / 60.0f;

    // Convert to angles
    float secAngle = sec * 6.0f;       // 360 / 60
    float minAngle = min * 6.0f;
    float hourAngle = hour * 30.0f;    // 360 / 12

    drawClockFace();
    drawHand(clockRadius * 0.5f, 6.0f, hourAngle, 0.2f, 0.2f, 0.2f);   // Hour hand
    drawHand(clockRadius * 0.7f, 4.0f, minAngle, 0.2f, 0.2f, 1.0f);    // Minute hand
    drawHand(clockRadius * 0.9f, 2.0f, secAngle, 1.0f, 0.0f, 0.0f);    // Second hand

    glutSwapBuffers();
}

// Timer callback for smooth updates
void timer(int) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0); // roughly 60 FPS
}

// Initialization
void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gluOrtho2D(-1, 1, -1, 1);
}

// Main function
int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Real-Time Analog Clock");
    init();
    glutDisplayFunc(display);
    glutTimerFunc(0, timer, 0);
    glutMainLoop();
    return 0;
}
//g++ clock.cpp -o clock.exe -lfreeglut -lglew32 -lopengl32 -lglu32