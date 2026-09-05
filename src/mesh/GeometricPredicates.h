#pragma once

#include <glm/glm.hpp>

// Fast, near-exact geometric orientation predicates.
//
// These use double-precision floating-point evaluation of the standard
// determinant forms. They are NOT adaptive-exact (Shewchuk) predicates:
// the sign is reliable away from degeneracy, but near-collinear /
// near-cocircular inputs can return a wrong sign. That trade is deliberate --
// this engine favours speed for triangulation/animation work over the
// exactness a CAD boolean kernel would require. If a future feature needs a
// robust boolean/CSG kernel, swap in an adaptive-exact implementation here
// (the call sites depend only on the sign contract below).
namespace GeometricPredicates {

    // Returns >0 if C is left of line AB, <0 if C is right of AB, =0 if collinear.
    double Orient2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c);

    // Returns >0 if D lies inside circumcircle of triangle ABC (CCW order), <0 if outside, =0 if cocircular.
    double InCircle2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d);

} // namespace GeometricPredicates
