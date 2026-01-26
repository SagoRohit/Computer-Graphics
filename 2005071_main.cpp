#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include "2005071_header.h"
#include "bitmap_image.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define M_PI 3.14159265358979323846

std::vector<Object *> objects;
std::vector<PointLight> pointLights;
std::vector<SpotLight> spotLights;
int imageW, imageH;
int recursionLevel = 2;
bool useTexture = true;
unsigned char *textureData = nullptr;
int textureWidth = 0, textureHeight = 0, textureChannels = 0;

Vector3D eye(0, 0, 100), u(0, 1, 0), r(1, 0, 0), l(0, 0, -1);
double moveDelta = 5, rotDelta = 3;
const double viewAngle = 80.0;

void loadData()
{
    std::ifstream in("scene.txt");
    if (!in)
    {
        std::cerr << "scene.txt not found\n";
        exit(1);
    }

    in >> recursionLevel;
    in >> imageW;
    imageH = imageW;

    int objCnt;
    in >> objCnt;
    std::string type;
    for (int k = 0; k < objCnt; ++k)
    {
        in >> type;
        if (type == "sphere")
        {
            double x, y, z, r, col[3], coef[4];
            int shine;
            in >> x >> y >> z >> r;
            for (int i = 0; i < 3; ++i)
                in >> col[i];
            for (int i = 0; i < 4; ++i)
                in >> coef[i];
            in >> shine;
            Sphere *s = new Sphere(Vector3D(x, y, z), r);
            for (int i = 0; i < 3; ++i)
                s->color[i] = col[i];
            for (int i = 0; i < 4; ++i)
                s->coEfficients[i] = coef[i];
            s->shine = shine;
            objects.push_back(s);
        }
        else if (type == "triangle")
        {
            double x1, y1, z1, x2, y2, z2, x3, y3, z3, col[3], coef[4];
            int shine;
            in >> x1 >> y1 >> z1 >> x2 >> y2 >> z2 >> x3 >> y3 >> z3;
            for (int i = 0; i < 3; ++i)
                in >> col[i];
            for (int i = 0; i < 4; ++i)
                in >> coef[i];
            in >> shine;
            Triangle *t = new Triangle(Vector3D(x1, y1, z1), Vector3D(x2, y2, z2), Vector3D(x3, y3, z3));
            for (int i = 0; i < 3; ++i)
                t->color[i] = col[i];
            for (int i = 0; i < 4; ++i)
                t->coEfficients[i] = coef[i];
            t->shine = shine;
            objects.push_back(t);
        }
        else if (type == "general")
        {
            double coeff[10], refx, refy, refz, l, w, h, col[3], coef[4];
            int shine;
            for (int i = 0; i < 10; ++i)
                in >> coeff[i];
            in >> refx >> refy >> refz >> l >> w >> h;
            for (int i = 0; i < 3; ++i)
                in >> col[i];
            for (int i = 0; i < 4; ++i)
                in >> coef[i];
            in >> shine;
            GeneralQuadric *gq = new GeneralQuadric(coeff, Vector3D(refx, refy, refz), l, w, h);
            for (int i = 0; i < 3; ++i)
                gq->color[i] = col[i];
            for (int i = 0; i < 4; ++i)
                gq->coEfficients[i] = coef[i];
            gq->shine = shine;
            objects.push_back(gq);
        }
    }

    Floor *floor = new Floor(1000, 20);
    floor->color[0] = floor->color[1] = floor->color[2] = 0.5;
    floor->coEfficients[0] = 0.4;
    floor->coEfficients[1] = 0.2;
    floor->coEfficients[2] = 0.2;
    floor->coEfficients[3] = 0.2;
    floor->shine = 5;
    objects.push_back(floor);

    int plCnt;
    in >> plCnt;
    for (int i = 0; i < plCnt; ++i)
    {
        double x, y, z, col[3];
        in >> x >> y >> z;
        for (int j = 0; j < 3; ++j)
            in >> col[j];
        PointLight pl;
        pl.position = Vector3D(x, y, z);
        for (int j = 0; j < 3; ++j)
            pl.color[j] = col[j];
        pointLights.push_back(pl);
    }

    int slCnt;
    in >> slCnt;
    for (int i = 0; i < slCnt; ++i)
    {
        double x, y, z, col[3], dx, dy, dz, cutoff;
        in >> x >> y >> z;
        for (int j = 0; j < 3; ++j)
            in >> col[j];
        in >> dx >> dy >> dz >> cutoff;
        SpotLight sl;
        sl.point_light.position = Vector3D(x, y, z);
        for (int j = 0; j < 3; ++j)
            sl.point_light.color[j] = col[j];
        sl.direction = Vector3D(dx, dy, dz);
        sl.cutoff_angle = cutoff;
        spotLights.push_back(sl);
    }
}

void move(const Vector3D &dir, double delta) { eye = eye + dir * delta; }

Vector3D rotate(const Vector3D &a, const Vector3D &axis, double angle)
{
    double rad = angle * M_PI / 180.0;
    Vector3D n = axis.normalize();
    return a * cos(rad) + n.cross(a) * sin(rad) + n * (n.dot(a)) * (1 - cos(rad));
}

void rotateYaw(double angle)
{
    l = rotate(l, u, angle).normalize();
    r = rotate(r, u, angle).normalize();
}
void rotatePitch(double angle)
{
    l = rotate(l, r, angle).normalize();
    u = rotate(u, r, angle).normalize();
}
void rotateRoll(double angle)
{
    u = rotate(u, l, angle).normalize();
    r = rotate(r, l, angle).normalize();
}
void capture()
{
    double planeDist = (imageH / 2.0) / tan(viewAngle / 2.0 * M_PI / 180.0);

    Vector3D topleft = eye + l * planeDist - r * (imageW / 2.0) + u * (imageH / 2.0);
    double du = 1.0;
    double dv = 1.0;

    bitmap_image image(imageW, imageH);
    image.set_all_channels(0);

    for (int i = 0; i < imageW; ++i)
    {
        for (int j = 0; j < imageH; ++j)
        {
            Vector3D pixel = topleft + r * i * du - u * j * dv;
            Vector3D dir = (pixel - eye).normalize();
            Ray ray(eye, dir);

            double tMin = 1e18;
            double nearestColor[3] = {0, 0, 0};
            for (Object *obj : objects)
            {
                double tempColor[3] = {0, 0, 0};
                double t = obj->intersect(ray, tempColor, 1);
                if (t > 0 && t < tMin)
                {
                    tMin = t;
                    nearestColor[0] = tempColor[0];
                    nearestColor[1] = tempColor[1];
                    nearestColor[2] = tempColor[2];
                }
            }
            image.set_pixel(i, j,
                            (unsigned char)(std::min(1.0, nearestColor[0]) * 255),
                            (unsigned char)(std::min(1.0, nearestColor[1]) * 255),
                            (unsigned char)(std::min(1.0, nearestColor[2]) * 255));
        }
    }
    static int imageSerial = 1;
    char filename[32];
    sprintf(filename, "Output_%02d.bmp", imageSerial++);
    image.save_image(filename);
    std::cout << "Image saved: " << filename << std::endl;
    std::cout << "useTexture=" << useTexture << std::endl;
}

void keyboard(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'w':
        move(l, moveDelta);
        break;
    case 's':
        move(l, -moveDelta);
        break;
    case 'a':
        move(r, -moveDelta);
        break;
    case 'd':
        move(r, moveDelta);
        break;
    case 'q':
        move(u, moveDelta);
        break;
    case 'e':
        move(u, -moveDelta);
        break;

    case '1':
        rotateYaw(rotDelta);
        break;
    case '2':
        rotateYaw(-rotDelta);
        break;
    case '3':
        rotatePitch(rotDelta);
        break;
    case '4':
        rotatePitch(-rotDelta);
        break;
    case '5':
        rotateRoll(rotDelta);
        break;
    case '6':
        rotateRoll(-rotDelta);
        break;
    case '0':
        capture();
        break;

    case 27:
        exit(0);
        break;
    }
    glutPostRedisplay();
}
void special(int key, int x, int y)
{
    switch (key)
    {
    case GLUT_KEY_UP:
        move(l, moveDelta);
        break;
    case GLUT_KEY_DOWN:
        move(l, -moveDelta);
        break;
    case GLUT_KEY_LEFT:
        move(r, -moveDelta);
        break;
    case GLUT_KEY_RIGHT:
        move(r, moveDelta);
        break;
    case GLUT_KEY_PAGE_UP:
        move(u, moveDelta);
        break;
    case GLUT_KEY_PAGE_DOWN:
        move(u, -moveDelta);
        break;
    }
    glutPostRedisplay();
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    Vector3D center = eye + l;
    gluLookAt(eye.x, eye.y, eye.z, center.x, center.y, center.z, u.x, u.y, u.z);

    for (auto obj : objects)
        obj->draw();

    for (auto &pl : pointLights)
    {
        glPushMatrix();
        glColor3dv(pl.color);
        glTranslatef(pl.position.x, pl.position.y, pl.position.z);
        glutSolidSphere(2, 10, 10);
        glPopMatrix();
    }

    glutSwapBuffers();
}

void initGL()
{
    glClearColor(0, 0, 0, 1);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(viewAngle, 1.0, 1, 10000);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv)
{
    loadData();
    stbi_set_flip_vertically_on_load(1);
    textureData = stbi_load("s1.bmp", &textureWidth, &textureHeight, &textureChannels, 3);
    if (!textureData)
    {
        std::cerr << "Failed to load texture!\n";
        exit(1);
    }
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(700, 700);
    glutCreateWindow("Ray Tracing Viewer");
    initGL();

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);

    glutMainLoop();
    for (auto obj : objects)
        delete obj;
    return 0;
}
