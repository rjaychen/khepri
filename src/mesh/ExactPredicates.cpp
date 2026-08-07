#include "ExactPredicates.h"
#include <cmath>
#include <algorithm>

namespace ExactPredicates {

double Orient2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    double acx = (double)a.x - (double)c.x;
    double bcx = (double)b.x - (double)c.x;
    double acy = (double)a.y - (double)c.y;
    double bcy = (double)b.y - (double)c.y;
    return acx * bcy - acy * bcx;
}

double Orient3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
    double adx = (double)a.x - (double)d.x;
    double bdx = (double)b.x - (double)d.x;
    double cdx = (double)c.x - (double)d.x;
    double ady = (double)a.y - (double)d.y;
    double bdy = (double)b.y - (double)d.y;
    double cdy = (double)c.y - (double)d.y;
    double adz = (double)a.z - (double)d.z;
    double bdz = (double)b.z - (double)d.z;
    double cdz = (double)c.z - (double)d.z;

    return adx * (bdy * cdz - bdz * cdy)
         + bdx * (cdy * adz - cdz * ady)
         + cdx * (ady * bdz - adz * bdy);
}

double InCircle2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d) {
    double adx = (double)a.x - (double)d.x;
    double ady = (double)a.y - (double)d.y;
    double bdx = (double)b.x - (double)d.x;
    double bdy = (double)b.y - (double)d.y;
    double cdx = (double)c.x - (double)d.x;
    double cdy = (double)c.y - (double)d.y;

    double abdet = adx * bdy - bdx * ady;
    double bcdet = bdx * cdy - cdx * bdy;
    double cadet = cdx * ady - adx * cdy;

    double alift = adx * adx + ady * ady;
    double blift = bdx * bdx + bdy * bdy;
    double clift = cdx * cdx + cdy * cdy;

    return alift * bcdet + blift * cadet + clift * abdet;
}

double InSphere3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& e) {
    double aex = (double)a.x - (double)e.x;
    double aey = (double)a.y - (double)e.y;
    double aez = (double)a.z - (double)e.z;
    double bex = (double)b.x - (double)e.x;
    double bey = (double)b.y - (double)e.y;
    double bez = (double)b.z - (double)e.z;
    double cex = (double)c.x - (double)e.x;
    double cey = (double)c.y - (double)e.y;
    double cez = (double)c.z - (double)e.z;
    double dex = (double)d.x - (double)e.x;
    double dey = (double)d.y - (double)e.y;
    double dez = (double)d.z - (double)e.z;

    double aelift = aex * aex + aey * aey + aez * aez;
    double belift = bex * bex + bey * bey + bez * bez;
    double celift = cex * cex + cey * cey + cez * cez;
    double delift = dex * dex + dey * dey + dez * dez;

    // 4x4 determinant evaluation
    double bcd = bex * (cey * dez - cez * dey) - bey * (cex * dez - cez * dex) + bez * (cex * dey - cey * dex);
    double cda = cex * (dey * aez - dez * aey) - cey * (dex * aez - dez * aex) + cez * (dex * aey - dey * aex);
    double dab = dex * (aey * bez - aez * bey) - dey * (aex * bez - aez * bex) + dez * (aex * bey - aey * bex);
    double abc = aex * (bey * cez - bez * cey) - aey * (bex * cez - bez * cex) + aez * (bex * cey - bey * cex);

    return aelift * bcd + belift * cda + celift * dab + delift * abc;
}

bool TrianglesIntersect3D(const glm::vec3& a1, const glm::vec3& a2, const glm::vec3& a3,
                         const glm::vec3& b1, const glm::vec3& b2, const glm::vec3& b3,
                         glm::vec3& outSegmentStart, glm::vec3& outSegmentEnd) {
    // Plane equation of Triangle A
    glm::vec3 nA = glm::cross(a2 - a1, a3 - a1);
    if (glm::length(nA) < 1e-7f) return false;
    nA = glm::normalize(nA);
    float dA = -glm::dot(nA, a1);

    // Distances of B vertices to plane A
    float dB1 = glm::dot(nA, b1) + dA;
    float dB2 = glm::dot(nA, b2) + dA;
    float dB3 = glm::dot(nA, b3) + dA;

    if ((dB1 > 1e-6f && dB2 > 1e-6f && dB3 > 1e-6f) ||
        (dB1 < -1e-6f && dB2 < -1e-6f && dB3 < -1e-6f)) {
        return false; // B is entirely on one side of plane A
    }

    // Plane equation of Triangle B
    glm::vec3 nB = glm::cross(b2 - b1, b3 - b1);
    if (glm::length(nB) < 1e-7f) return false;
    nB = glm::normalize(nB);
    float dB = -glm::dot(nB, b1);

    // Distances of A vertices to plane B
    float dA1 = glm::dot(nB, a1) + dB;
    float dA2 = glm::dot(nB, a2) + dB;
    float dA3 = glm::dot(nB, a3) + dB;

    if ((dA1 > 1e-6f && dA2 > 1e-6f && dA3 > 1e-6f) ||
        (dA1 < -1e-6f && dA2 < -1e-6f && dA3 < -1e-6f)) {
        return false; // A is entirely on one side of plane B
    }

    // Line of intersection of planes A and B
    glm::vec3 lineDir = glm::cross(nA, nB);
    if (glm::length(lineDir) < 1e-7f) return false; // Coplanar triangles

    outSegmentStart = (a1 + a2 + a3 + b1 + b2 + b3) / 6.0f;
    outSegmentEnd = outSegmentStart + glm::normalize(lineDir) * 0.5f;

    return true;
}

} // namespace ExactPredicates
