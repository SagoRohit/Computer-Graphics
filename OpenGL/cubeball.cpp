#include <GL/freeglut.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iostream>

const double PI = acos(-1.0);
const float GRAVITY = -9.8f;
const float RESTITUTION = 0.75f;
const float VELOCITY_THRESHOLD = 0.8f;

bool simulationRunning = false;
bool showVelocityArrow = false;
float initialSpeed = 10.0f;

void printInstructions() {
    std::cout << "========== CONTROLS ==========\n";
    std::cout << "[Arrow Keys]     : Move camera (Forward/Backward/Left/Right)\n";
    std::cout << "[Page Up/Down]   : Move camera up/down\n";
    std::cout << "[1/2]            : Look Left/Right (Yaw)\n";
    std::cout << "[3/4]            : Look Up/Down (Pitch)\n";
    std::cout << "[5/6]            : Tilt Clockwise/CounterClockwise (Roll)\n";
    std::cout << "[w/s]            : Move up/down without changing reference point\n";
    std::cout << "[Space]          : Toggle simulation ON/OFF\n";
    std::cout << "[r]              : Reset ball with random upward velocity\n";
    std::cout << "[+/-]            : Increase/decrease initial speed\n";
    std::cout << "[v]              : Toggle velocity arrow\n";
    std::cout << "[c]              : Reset camera\n";
    std::cout << "[ESC]            : Exit\n";
    std::cout << "==============================\n";
}

// ------------------------- Camera -------------------------
class Camera {
public:
    double posX, posY, posZ;
    double uX, uY, uZ;
    double rX, rY, rZ;
    double lX, lY, lZ;
    const double MOVE_CONSTANT = 1.0;
    const double ROTATE_CONSTANT = 2.0 * PI / 180.0;

    Camera() { reset(); }

    void reset() {
        double v = 1.0 / sqrt(2);
        posX = 60.0; posY = 60.0; posZ = 20.0;
        uX = 0.0; uY = 0.0; uZ = 1.0;
        rX = -v; rY = v; rZ = 0.0;
        lX = -v; lY = -v; lZ = 0.0;
    }

    void move(double x, double y, double z, bool pos_dir) {
        double dir = (pos_dir ? 1.0 : -1.0) * MOVE_CONSTANT;
        posX += x * dir;
        posY += y * dir;
        posZ += z * dir;
    }

    void rotate(double &p1X, double &p1Y, double &p1Z,
                double &p2X, double &p2Y, double &p2Z,
                bool pos_rotation) const {
        double angle = (pos_rotation ? 1.0 : -1.0) * ROTATE_CONSTANT;

        double temp1X = p1X * cos(angle) - p2X * sin(angle);
        double temp1Y = p1Y * cos(angle) - p2Y * sin(angle);
        double temp1Z = p1Z * cos(angle) - p2Z * sin(angle);
        double temp2X = p1X * sin(angle) + p2X * cos(angle);
        double temp2Y = p1Y * sin(angle) + p2Y * cos(angle);
        double temp2Z = p1Z * sin(angle) + p2Z * cos(angle);

        p1X = temp1X; p1Y = temp1Y; p1Z = temp1Z;
        p2X = temp2X; p2Y = temp2Y; p2Z = temp2Z;

        // Normalize
        double len1 = sqrt(p1X * p1X + p1Y * p1Y + p1Z * p1Z);
        double len2 = sqrt(p2X * p2X + p2Y * p2Y + p2Z * p2Z);
        if (len1 > 0) { p1X /= len1; p1Y /= len1; p1Z /= len1; }
        if (len2 > 0) { p2X /= len2; p2Y /= len2; p2Z /= len2; }
    }
};

Camera cam;

// ------------------------- Sphere + Physics -------------------------
class Sphere {
public:
    float x, y, z;
    float vx, vy, vz;
    float rotX, rotY, rotZ;
    float radius;

    Sphere(float r = 0.7f) : radius(r) { reset(); }

    void reset() {
        float limit = 20.0f - radius - 1.0f; 
        x = ((static_cast<float>(rand()) / RAND_MAX) * 2 - 1) * limit;
        y = ((static_cast<float>(rand()) / RAND_MAX) * 2 - 1) * limit;
        z = -18.0f;
    
        float angle = static_cast<float>(rand()) / RAND_MAX * 2 * PI;
        vx = initialSpeed * cos(angle);
        vy = initialSpeed * sin(angle);
        vz = initialSpeed;
    
        rotX = rotY = rotZ = 0;
    
        std::cout << "Ball reset at (" << x << ", " << y << ", " << z << ") with speed: " << initialSpeed << "\n";
    }
    
    void update(float dt) {
        if (!simulationRunning) return;
    
        // Apply gravity
        vz += GRAVITY * dt;
    
        // Update position
        x += vx * dt;
        y += vy * dt;
        z += vz * dt;
    
        // Rolling rotation effect
        float speed = sqrt(vx * vx + vy * vy + vz * vz);
        if (speed > 0.0f) {
            float angle = (speed / radius) * dt * (180.0f / PI);
            rotX += angle * vy / speed;
            rotY += angle * vz / speed;
            rotZ += angle * vx / speed;
        }
    
        float limit = 20.0f - radius;
        const float surfaceDamping = 0.98f;  
        const float bounceRestitution = 0.82f; 
    
        // Wall collisions with slight horizontal damping
        if (x <= -limit) {
            x = -limit;
            vx *= -bounceRestitution;
            vy *= surfaceDamping;
        }
        else if (x >= limit) {
            x = limit;
            vx *= -bounceRestitution;
            vy *= surfaceDamping;
        }
    
        if (y <= -limit) {
            y = -limit;
            vy *= -bounceRestitution;
            vx *= surfaceDamping;
        }
        else if (y >= limit) {
            y = limit;
            vy *= -bounceRestitution;
            vx *= surfaceDamping;
        }
    
        // Floor and ceiling
        if (z <= -limit) {
            z = -limit;
            if (fabs(vz) < 0.3f)  
                vz = 0;
            else
                vz *= -bounceRestitution;
        }
        else if (z >= limit) {
            z = limit;
            vz *= -bounceRestitution;
        }
    }
    

    void draw() {
        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(rotZ, 0, 0, 1);
        glRotatef(rotY, 0, 1, 0);
        glRotatef(rotX, 1, 0, 0);
        glColor3f(1.0f, 0.0f, 1.0f); // New ball color
        glutSolidSphere(radius, 40, 40);

        if (showVelocityArrow) {
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex3f(0, 0, 0);
            glVertex3f(vx, vy, vz);
            glEnd();
        }
        glPopMatrix();
    }
};

Sphere sphere;

void drawCheckeredFloor() {
    float size = 40.0f;
    int tiles = 20;
    float tileSize = size / tiles;
    glBegin(GL_QUADS);
    for (int i = -tiles / 2; i < tiles / 2; ++i) {
        for (int j = -tiles / 2; j < tiles / 2; ++j) {
            glColor3f((i + j) % 2 ? 0.2f : 1.0f, (i + j) % 2 ? 0.2f : 1.0f, (i + j) % 2 ? 0.2f : 1.0f);
            glVertex3f(i * tileSize, j * tileSize, -20.0f);
            glVertex3f((i + 1) * tileSize, j * tileSize, -20.0f);
            glVertex3f((i + 1) * tileSize, (j + 1) * tileSize, -20.0f);
            glVertex3f(i * tileSize, (j + 1) * tileSize, -20.0f);
        }
    }
    glEnd();
}

void drawColoredCube() {
    float s = 20.0f;
    glBegin(GL_QUADS);
    glColor3f(1, 0, 0); glVertex3f(-s, -s, s); glVertex3f(s, -s, s); glVertex3f(s, s, s); glVertex3f(-s, s, s); // Front
    glColor3f(0.3f, 0.5f, 0.1f); glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(s, -s, s); glVertex3f(s, -s, -s); // Back
    glColor3f(0.2f, 0.3f, 0.5f); glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(-s, s, s); glVertex3f(-s, s, -s); // Left
    glColor3f(1, 1, 0); glVertex3f(s, -s, -s); glVertex3f(s, -s, s); glVertex3f(s, s, s); glVertex3f(s, s, -s); // Right
    glColor3f(0, 1, 1); glVertex3f(-s, s, -s); glVertex3f(s, s, -s); glVertex3f(s, s, s); glVertex3f(-s, s, s); // Top
    glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(cam.posX, cam.posY, cam.posZ, cam.posX + cam.lX, cam.posY + cam.lY, cam.posZ + cam.lZ, cam.uX, cam.uY, cam.uZ);
    drawCheckeredFloor();
    drawColoredCube();
    sphere.draw();
    glutSwapBuffers();
}

void idle() {
    static int lastTime = glutGet(GLUT_ELAPSED_TIME);
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float dt = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;
    sphere.update(dt);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 27: exit(0);
        case '1': cam.rotate(cam.lX, cam.lY, cam.lZ, cam.rX, cam.rY, cam.rZ, true); break;
        case '2': cam.rotate(cam.lX, cam.lY, cam.lZ, cam.rX, cam.rY, cam.rZ, false); break;
        case '3': cam.rotate(cam.lX, cam.lY, cam.lZ, cam.uX, cam.uY, cam.uZ, false); break;
        case '4': cam.rotate(cam.lX, cam.lY, cam.lZ, cam.uX, cam.uY, cam.uZ, true); break;
        case '5': cam.rotate(cam.rX, cam.rY, cam.rZ, cam.uX, cam.uY, cam.uZ, true); break;
        case '6': cam.rotate(cam.rX, cam.rY, cam.rZ, cam.uX, cam.uY, cam.uZ, false); break;
        case 'w': cam.move(cam.uX, cam.uY, cam.uZ, true); break;
        case 's': cam.move(cam.uX, cam.uY, cam.uZ, false); break;
        case 'c': cam.reset(); std::cout << "Camera reset.\n"; break;
        case ' ': simulationRunning = !simulationRunning; std::cout << "Simulation: " << (simulationRunning ? "ON\n" : "OFF\n"); break;
        case 'v': showVelocityArrow = !showVelocityArrow; break;
        case 'r': sphere.reset(); break;
        case '+': initialSpeed += 1.0f; std::cout << "Initial speed increased to " << initialSpeed << "\n"; break;
        case '-': initialSpeed = std::max(0.0f, initialSpeed - 1.0f); std::cout << "Initial speed decreased to " << initialSpeed << "\n"; break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int, int) {
    switch (key) {
        case GLUT_KEY_UP: cam.move(cam.lX, cam.lY, cam.lZ, true); break;
        case GLUT_KEY_DOWN: cam.move(cam.lX, cam.lY, cam.lZ, false); break;
        case GLUT_KEY_LEFT: cam.move(cam.rX, cam.rY, cam.rZ, false); break;
        case GLUT_KEY_RIGHT: cam.move(cam.rX, cam.rY, cam.rZ, true); break;
        case GLUT_KEY_PAGE_UP: cam.move(cam.uX, cam.uY, cam.uZ, true); break;
        case GLUT_KEY_PAGE_DOWN: cam.move(cam.uX, cam.uY, cam.uZ, false); break;
    }
    glutPostRedisplay();
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 1.0, 1.0, 1000.0);
    srand(static_cast<unsigned int>(time(nullptr)));
    printInstructions();
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 800);
    glutCreateWindow("Bouncing Sphere in Cube with Camera");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}
////g++ sp.cpp -o sp.exe -lfreeglut -lglew32 -lopengl32 -lglu32