#pragma once
#include <vector>
#include <cmath>
#include <iostream>
#include <GL/glut.h>

struct Vector3D
{
    double x, y, z;
    Vector3D(double x_ = 0, double y_ = 0, double z_ = 0) : x(x_), y(y_), z(z_) {}
    Vector3D operator+(const Vector3D &b) const { return Vector3D(x + b.x, y + b.y, z + b.z); }
    Vector3D operator-(const Vector3D &b) const { return Vector3D(x - b.x, y - b.y, z - b.z); }
    Vector3D operator*(double s) const { return Vector3D(x * s, y * s, z * s); }
    double dot(const Vector3D &b) const { return x * b.x + y * b.y + z * b.z; }
    Vector3D cross(const Vector3D &b) const
    {
        return Vector3D(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);
    }
    Vector3D normalize() const
    {
        double mag = std::sqrt(x * x + y * y + z * z);
        return (mag > 1e-8) ? (*this) * (1.0 / mag) : Vector3D(0, 0, 0);
    }
};
struct PointLight
{
    Vector3D position;
    double color[3];
};

struct SpotLight
{
    PointLight point_light;
    Vector3D direction;
    double cutoff_angle;
};

struct Ray
{
    Vector3D start, dir;
    Ray(const Vector3D &s, const Vector3D &d) : start(s), dir(d.normalize()) {}
};
struct Color
{
    double r, g, b;
};
extern unsigned char *textureData;
extern int textureWidth, textureHeight, textureChannels;

Color sampleTexture(double u, double v)
{
    if (!textureData || textureWidth <= 0 || textureHeight <= 0)
    {
        return Color{0.5, 0.5, 0.5};
    }
    u = std::max(0.0, std::min(1.0, u));
    v = std::max(0.0, std::min(1.0, v));
    int pixel_x = (int)(u * (textureWidth - 1));
    int pixel_y = (int)((1.0 - v) * (textureHeight - 1));
    pixel_x = std::max(0, std::min(textureWidth - 1, pixel_x));
    pixel_y = std::max(0, std::min(textureHeight - 1, pixel_y));
    int index = (pixel_y * textureWidth + pixel_x) * textureChannels;
    int max_index = textureWidth * textureHeight * textureChannels;
    if (index < 0 || index + 2 >= max_index)
        return Color{1.0, 0.0, 1.0};
    Color color;
    color.r = textureData[index] / 255.0;
    color.g = textureChannels >= 2 ? textureData[index + 1] / 255.0 : color.r;
    color.b = textureChannels >= 3 ? textureData[index + 2] / 255.0 : color.r;
    return color;
}

class Object
{
public:
    Vector3D reference_point;
    double height, width, length;
    double color[3];
    double coEfficients[4];
    int shine;

    Object() {}
    virtual void draw() const = 0;
    virtual double intersect(const Ray &, double *colorOut, int level) { return -1.0; }
    virtual ~Object() {}
};
extern bool useTexture;
extern std::vector<Object *> objects;
extern std::vector<PointLight> pointLights;
extern std::vector<SpotLight> spotLights;
extern int recursionLevel;

class Sphere : public Object
{
public:
    Sphere(const Vector3D &center, double radius)
    {
        reference_point = center;
        length = radius;
        color[0] = color[1] = color[2] = 1.0;
        for (int i = 0; i < 4; i++)
            coEfficients[i] = 0.0;
        shine = 1;
    }
    void draw() const override
    {
        glPushMatrix();
        glColor3dv(color);
        glTranslatef(reference_point.x, reference_point.y, reference_point.z);
        glutSolidSphere(length, 24, 24);
        glPopMatrix();
    }

    double intersect(const Ray &ray, double *colorOut, int level) override
    {
        Vector3D oc = ray.start - reference_point;
        double a = ray.dir.dot(ray.dir);
        double b = 2 * ray.dir.dot(oc);
        double c = oc.dot(oc) - length * length;
        double disc = b * b - 4 * a * c;
        if (disc < 0)
            return -1;
        double t1 = (-b - sqrt(disc)) / (2 * a);
        double t2 = (-b + sqrt(disc)) / (2 * a);
        double t = 1e18;
        if (t1 > 1e-6)
            t = t1;
        else if (t2 > 1e-6)
            t = t2;
        else
            return -1;
        if (level == 0)
            return t;
        Vector3D intersection = ray.start + ray.dir * t;
        Vector3D normal = (intersection - reference_point).normalize();
        double objColor[3] = {color[0], color[1], color[2]};
        double finalColor[3] = {
            objColor[0] * coEfficients[0],
            objColor[1] * coEfficients[0],
            objColor[2] * coEfficients[0]};

        for (const auto &light : pointLights)
        {
            Vector3D toLight = (light.position - intersection).normalize();
            Vector3D shadowStart = intersection + toLight * 1e-3;
            Ray shadowRay(shadowStart, toLight);
            bool inShadow = false;
            for (auto obj : objects)
            {
                if (obj == this)
                    continue;
                double tempColor[3];
                double t_shadow = obj->intersect(shadowRay, tempColor, 0);
                if (t_shadow > 1e-6 && t_shadow < (light.position - intersection).dot(toLight))
                {
                    inShadow = true;
                    break;
                }
            }
            if (inShadow)
                continue;
            double lambert = std::max(0.0, normal.dot(toLight));
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[1] * objColor[i] * lambert;
            Vector3D toEye = (ray.start - intersection).normalize();
            Vector3D reflectDir = (normal * 2.0 * normal.dot(toLight) - toLight).normalize();
            double phong = std::pow(std::max(0.0, toEye.dot(reflectDir)), shine);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[2] * phong;
        }

        if (level < recursionLevel && coEfficients[3] > 0.0)
        {
            Vector3D reflDir = (ray.dir - normal * 2.0 * normal.dot(ray.dir)).normalize();
            Vector3D reflStart = intersection + reflDir * 1e-3;
            Ray reflRay(reflStart, reflDir);
            double reflColor[3] = {0, 0, 0};
            double minT = 1e18;
            Object *nearest = nullptr;
            for (Object *obj : objects)
            {
                double t_ref = obj->intersect(reflRay, nullptr, 0);
                if (t_ref > 1e-6 && t_ref < minT)
                {
                    minT = t_ref;
                    nearest = obj;
                }
            }
            if (nearest)
                nearest->intersect(reflRay, reflColor, level + 1);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += reflColor[i] * coEfficients[3];
        }
        for (int i = 0; i < 3; ++i)
            colorOut[i] = std::min(1.0, std::max(0.0, finalColor[i]));
        return t;
    }
};

class Triangle : public Object
{
public:
    Vector3D p1, p2, p3;
    Vector3D normal;

    Triangle(Vector3D a, Vector3D b, Vector3D c) : p1(a), p2(b), p3(c)
    {

        normal = (p2 - p1).cross(p3 - p1).normalize();
        color[0] = color[1] = color[2] = 1.0;
        for (int i = 0; i < 4; i++)
            coEfficients[i] = 0.0;
        shine = 1;
    }

    void draw() const override
    {
        glColor3dv(color);
        glBegin(GL_TRIANGLES);
        glVertex3d(p1.x, p1.y, p1.z);
        glVertex3d(p2.x, p2.y, p2.z);
        glVertex3d(p3.x, p3.y, p3.z);
        glEnd();
    }

    double intersect(const Ray &ray, double *colorOut, int level) override
    {
        const double EPS = 1e-6;
        Vector3D edge1 = p2 - p1;
        Vector3D edge2 = p3 - p1;
        Vector3D h = ray.dir.cross(edge2);
        double a = edge1.dot(h);
        if (fabs(a) < EPS)
            return -1;

        double f = 1.0 / a;
        Vector3D s = ray.start - p1;
        double u = f * s.dot(h);
        if (u < 0.0 || u > 1.0)
            return -1;

        Vector3D q = s.cross(edge1);
        double v = f * ray.dir.dot(q);
        if (v < 0.0 || u + v > 1.0)
            return -1;

        double t = f * edge2.dot(q);
        if (t < EPS)
            return -1;

        if (level == 0)
            return t;

        Vector3D intersection = ray.start + ray.dir * t;
        Vector3D n = normal;
        double objColor[3] = {color[0], color[1], color[2]};
        double finalColor[3] = {
            objColor[0] * coEfficients[0],
            objColor[1] * coEfficients[0],
            objColor[2] * coEfficients[0]};

        for (const auto &light : pointLights)
        {
            Vector3D toLight = (light.position - intersection).normalize();
            Vector3D shadowStart = intersection + toLight * 1e-3;
            Ray shadowRay(shadowStart, toLight);
            bool inShadow = false;
            for (auto obj : objects)
            {
                if (obj == this)
                    continue;
                double tempColor[3];
                double t_shadow = obj->intersect(shadowRay, tempColor, 0);
                if (t_shadow > 1e-6 && t_shadow < (light.position - intersection).dot(toLight))
                {
                    inShadow = true;
                    break;
                }
            }
            if (inShadow)
                continue;
            double lambert = std::max(0.0, n.dot(toLight));
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[1] * objColor[i] * lambert;
            Vector3D toEye = (ray.start - intersection).normalize();
            Vector3D reflectDir = (n * 2.0 * n.dot(toLight) - toLight).normalize();
            double phong = std::pow(std::max(0.0, toEye.dot(reflectDir)), shine);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[2] * phong;
        }

        if (level < recursionLevel && coEfficients[3] > 0.0)
        {
            Vector3D reflDir = (ray.dir - n * 2.0 * n.dot(ray.dir)).normalize();
            Vector3D reflStart = intersection + reflDir * 1e-3;
            Ray reflRay(reflStart, reflDir);
            double reflColor[3] = {0, 0, 0};
            double minT = 1e18;
            Object *nearest = nullptr;
            for (Object *obj : objects)
            {
                double t_ref = obj->intersect(reflRay, nullptr, 0);
                if (t_ref > 1e-6 && t_ref < minT)
                {
                    minT = t_ref;
                    nearest = obj;
                }
            }
            if (nearest)
                nearest->intersect(reflRay, reflColor, level + 1);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += reflColor[i] * coEfficients[3];
        }
        for (int i = 0; i < 3; ++i)
            colorOut[i] = std::min(1.0, std::max(0.0, finalColor[i]));
        return t;
    }
};

class GeneralQuadric : public Object
{
public:
    double params[10];
    Vector3D cube_ref;
    double cube_len, cube_wid, cube_hei;

    GeneralQuadric(const double *coeffs, const Vector3D &ref, double l, double w, double h)
    {
        for (int i = 0; i < 10; ++i)
            params[i] = coeffs[i];
        cube_ref = ref;
        cube_len = l;
        cube_wid = w;
        cube_hei = h;
        color[0] = color[1] = color[2] = 1.0;
        for (int i = 0; i < 4; i++)
            coEfficients[i] = 0.0;
        shine = 1;
    }

    void draw() const override {}

    bool insideCube(const Vector3D &p) const
    {
        if (cube_len > 0 && (p.x < cube_ref.x || p.x > cube_ref.x + cube_len))
            return false;
        if (cube_wid > 0 && (p.y < cube_ref.y || p.y > cube_ref.y + cube_wid))
            return false;
        if (cube_hei > 0 && (p.z < cube_ref.z || p.z > cube_ref.z + cube_hei))
            return false;
        return true;
    }

    double intersect(const Ray &ray, double *colorOut, int level) override
    {

        double sx = ray.start.x, sy = ray.start.y, sz = ray.start.z;
        double dx = ray.dir.x, dy = ray.dir.y, dz = ray.dir.z;
        double A = params[0], B = params[1], C = params[2];
        double D = params[3], E = params[4], F = params[5];
        double G = params[6], H = params[7], I = params[8], J = params[9];

        double a = A * dx * dx + B * dy * dy + C * dz * dz + D * dx * dy + E * dx * dz + F * dy * dz;
        double b = 2 * A * sx * dx + 2 * B * sy * dy + 2 * C * sz * dz + D * (sx * dy + sy * dx) + E * (sx * dz + sz * dx) + F * (sy * dz + sz * dy) + G * dx + H * dy + I * dz;
        double c = A * sx * sx + B * sy * sy + C * sz * sz + D * sx * sy + E * sx * sz + F * sy * sz + G * sx + H * sy + I * sz + J;

        double disc = b * b - 4 * a * c;
        if (fabs(a) < 1e-9 || disc < 0)
            return -1;

        double sqrt_disc = sqrt(disc);
        double t1 = (-b - sqrt_disc) / (2 * a);
        double t2 = (-b + sqrt_disc) / (2 * a);

        double t = 1e18;
        bool found = false;
        for (double tCand : {t1, t2})
        {
            if (tCand > 1e-6)
            {
                Vector3D p = ray.start + ray.dir * tCand;
                if (insideCube(p))
                {
                    if (tCand < t)
                    {
                        t = tCand;
                        found = true;
                    }
                }
            }
        }
        if (!found)
            return -1;
        if (level == 0)
            return t;

        Vector3D intersection = ray.start + ray.dir * t;

        double nx = 2 * A * intersection.x + D * intersection.y + E * intersection.z + G;
        double ny = 2 * B * intersection.y + D * intersection.x + F * intersection.z + H;
        double nz = 2 * C * intersection.z + E * intersection.x + F * intersection.y + I;
        Vector3D normal(nx, ny, nz);
        normal = normal.normalize();

        double objColor[3] = {color[0], color[1], color[2]};
        double finalColor[3] = {
            objColor[0] * coEfficients[0],
            objColor[1] * coEfficients[0],
            objColor[2] * coEfficients[0]};

        for (const auto &light : pointLights)
        {
            Vector3D toLight = (light.position - intersection).normalize();
            Vector3D shadowStart = intersection + toLight * 1e-3;
            Ray shadowRay(shadowStart, toLight);
            bool inShadow = false;
            for (auto obj : objects)
            {
                if (obj == this)
                    continue;
                double tempColor[3];
                double t_shadow = obj->intersect(shadowRay, tempColor, 0);
                if (t_shadow > 1e-6 && t_shadow < (light.position - intersection).dot(toLight))
                {
                    inShadow = true;
                    break;
                }
            }
            if (inShadow)
                continue;
            double lambert = std::max(0.0, normal.dot(toLight));
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[1] * objColor[i] * lambert;
            Vector3D toEye = (ray.start - intersection).normalize();
            Vector3D reflectDir = (normal * 2.0 * normal.dot(toLight) - toLight).normalize();
            double phong = std::pow(std::max(0.0, toEye.dot(reflectDir)), shine);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[2] * phong;
        }

        if (level < recursionLevel && coEfficients[3] > 0.0)
        {
            Vector3D reflDir = (ray.dir - normal * 2.0 * normal.dot(ray.dir)).normalize();
            Vector3D reflStart = intersection + reflDir * 1e-3;
            Ray reflRay(reflStart, reflDir);
            double reflColor[3] = {0, 0, 0};
            double minT = 1e18;
            Object *nearest = nullptr;
            for (Object *obj : objects)
            {
                double t_ref = obj->intersect(reflRay, nullptr, 0);
                if (t_ref > 1e-6 && t_ref < minT)
                {
                    minT = t_ref;
                    nearest = obj;
                }
            }
            if (nearest)
                nearest->intersect(reflRay, reflColor, level + 1);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += reflColor[i] * coEfficients[3];
        }
        for (int i = 0; i < 3; ++i)
            colorOut[i] = std::min(1.0, std::max(0.0, finalColor[i]));
        return t;
    }
};

class Floor : public Object
{
public:
    double floorWidth, tileWidth;
    Floor(double floorWidth_, double tileWidth_)
    {
        reference_point = Vector3D(-floorWidth_ / 2.0, -floorWidth_ / 2.0, 0);
        floorWidth = floorWidth_;
        tileWidth = tileWidth_;
        color[0] = color[1] = color[2] = 0.5; // fallback color
        for (int i = 0; i < 4; i++)
            coEfficients[i] = 0.2;
        shine = 5;
    }
    void draw() const override
    {
        int numTiles = static_cast<int>(floorWidth / tileWidth);
        double z = reference_point.z;
        for (int i = 0; i < numTiles; ++i)
        {
            for (int j = 0; j < numTiles; ++j)
            {
                if ((i + j) % 2 == 0)
                    glColor3f(0.9f, 0.9f, 0.9f);
                else
                    glColor3f(0.1f, 0.1f, 0.1f);
                double x = reference_point.x + i * tileWidth;
                double y = reference_point.y + j * tileWidth;
                glBegin(GL_QUADS);
                glVertex3d(x, y, z);
                glVertex3d(x, y + tileWidth, z);
                glVertex3d(x + tileWidth, y + tileWidth, z);
                glVertex3d(x + tileWidth, y, z);
                glEnd();
            }
        }
    }

    void getColorAt(const Vector3D &point, double *outColor) const
    {
        int tileX = static_cast<int>((point.x - reference_point.x) / tileWidth);
        int tileY = static_cast<int>((point.y - reference_point.y) / tileWidth);

        if (!useTexture)
        {
            if ((tileX + tileY) % 2 == 0)
            { // white
                outColor[0] = outColor[1] = outColor[2] = 0.9;
            }
            else
            { // black
                outColor[0] = outColor[1] = outColor[2] = 0.1;
            }
        }
        else
        {

            double rel_x = point.x - reference_point.x;
            double rel_y = point.y - reference_point.y;

            int tileX = static_cast<int>(rel_x / tileWidth);
            int tileY = static_cast<int>(rel_y / tileWidth);

            double local_u = fmod(rel_x, tileWidth) / tileWidth;
            double local_v = fmod(rel_y, tileWidth) / tileWidth;
            if (local_u < 0)
                local_u += 1.0;
            if (local_v < 0)
                local_v += 1.0;

            Color texColor = sampleTexture(local_u, local_v);
            outColor[0] = texColor.r;
            outColor[1] = texColor.g;
            outColor[2] = texColor.b;
        }
    }

    double intersect(const Ray &ray, double *colorOut, int level) override
    {
        const double EPS = 1e-6;

        if (fabs(ray.dir.z) < EPS)
            return -1; // parallel
        double t = -ray.start.z / ray.dir.z;
        if (t < EPS)
            return -1;

        Vector3D intersection = ray.start + ray.dir * t;
        // Check if within floor extent
        if (intersection.x < reference_point.x || intersection.x > reference_point.x + floorWidth ||
            intersection.y < reference_point.y || intersection.y > reference_point.y + floorWidth)
            return -1;
        if (level == 0)
            return t;

        // Get tile color or texture
        double objColor[3];
        getColorAt(intersection, objColor);

        // Phong lighting (normal = (0,0,1))
        Vector3D normal(0, 0, 1);
        double finalColor[3] = {
            objColor[0] * coEfficients[0],
            objColor[1] * coEfficients[0],
            objColor[2] * coEfficients[0]};

        // Point lights
        for (const auto &light : pointLights)
        {
            Vector3D toLight = (light.position - intersection).normalize();
            Vector3D shadowStart = intersection + toLight * 1e-3;
            Ray shadowRay(shadowStart, toLight);
            bool inShadow = false;
            for (auto obj : objects)
            {
                if (obj == this)
                    continue;
                double tempColor[3];
                double t_shadow = obj->intersect(shadowRay, tempColor, 0);
                if (t_shadow > 1e-6 && t_shadow < (light.position - intersection).dot(toLight))
                {
                    inShadow = true;
                    break;
                }
            }
            if (inShadow)
                continue;
            double lambert = std::max(0.0, normal.dot(toLight));
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[1] * objColor[i] * lambert;
            Vector3D toEye = (ray.start - intersection).normalize();
            Vector3D reflectDir = (normal * 2.0 * normal.dot(toLight) - toLight).normalize();
            double phong = std::pow(std::max(0.0, toEye.dot(reflectDir)), shine);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += light.color[i] * coEfficients[2] * phong;
        }

        // Reflection
        if (level < recursionLevel && coEfficients[3] > 0.0)
        {
            Vector3D reflDir = (ray.dir - normal * 2.0 * normal.dot(ray.dir)).normalize();
            Vector3D reflStart = intersection + reflDir * 1e-3;
            Ray reflRay(reflStart, reflDir);
            double reflColor[3] = {0, 0, 0};
            double minT = 1e18;
            Object *nearest = nullptr;
            for (Object *obj : objects)
            {
                double t_ref = obj->intersect(reflRay, nullptr, 0);
                if (t_ref > 1e-6 && t_ref < minT)
                {
                    minT = t_ref;
                    nearest = obj;
                }
            }
            if (nearest)
                nearest->intersect(reflRay, reflColor, level + 1);
            for (int i = 0; i < 3; ++i)
                finalColor[i] += reflColor[i] * coEfficients[3];
        }
        for (int i = 0; i < 3; ++i)
            colorOut[i] = std::min(1.0, std::max(0.0, finalColor[i]));
        return t;
    }
};
