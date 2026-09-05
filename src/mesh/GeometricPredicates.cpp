#include "GeometricPredicates.h"

namespace GeometricPredicates {

double Orient2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    const double acx = static_cast<double>(a.x) - static_cast<double>(c.x);
    const double bcx = static_cast<double>(b.x) - static_cast<double>(c.x);
    const double acy = static_cast<double>(a.y) - static_cast<double>(c.y);
    const double bcy = static_cast<double>(b.y) - static_cast<double>(c.y);
    return acx * bcy - acy * bcx;
}

double InCircle2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d) {
    const double adx = static_cast<double>(a.x) - static_cast<double>(d.x);
    const double ady = static_cast<double>(a.y) - static_cast<double>(d.y);
    const double bdx = static_cast<double>(b.x) - static_cast<double>(d.x);
    const double bdy = static_cast<double>(b.y) - static_cast<double>(d.y);
    const double cdx = static_cast<double>(c.x) - static_cast<double>(d.x);
    const double cdy = static_cast<double>(c.y) - static_cast<double>(d.y);

    const double abdet = adx * bdy - bdx * ady;
    const double bcdet = bdx * cdy - cdx * bdy;
    const double cadet = cdx * ady - adx * cdy;

    const double alift = adx * adx + ady * ady;
    const double blift = bdx * bdx + bdy * bdy;
    const double clift = cdx * cdx + cdy * cdy;

    return alift * bcdet + blift * cadet + clift * abdet;
}

} // namespace GeometricPredicates
