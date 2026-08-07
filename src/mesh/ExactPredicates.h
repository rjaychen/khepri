#pragma once

#include <glm/glm.hpp>

namespace ExactPredicates {

    // Returns >0 if C is left of line AB, <0 if C is right of AB, =0 if collinear
    double Orient2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c);

    // Returns >0 if D is above plane ABC (counter-clockwise order), <0 if below, =0 if coplanar
    double Orient3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d);

    // Returns >0 if D lies inside circumcircle of triangle ABC (CCW order), <0 if outside, =0 if cocircular
    double InCircle2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d);

    // Returns >0 if E lies inside circumsphere of tetrahedron ABCD, <0 if outside, =0 if cospherical
    double InSphere3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& e);

    // Exact Triangle-Triangle Intersection test in 3D
    bool TrianglesIntersect3D(const glm::vec3& a1, const glm::vec3& a2, const glm::vec3& a3,
                             const glm::vec3& b1, const glm::vec3& b2, const glm::vec3& b3,
                             glm::vec3& outSegmentStart, glm::vec3& outSegmentEnd);
}
