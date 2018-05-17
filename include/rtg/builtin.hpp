#ifndef RTG_GUARD_BUILTIN_HPP
#define RTG_GUARD_BUILTIN_HPP

#include <rtg/operation.hpp>
#include <rtg/errors.hpp>

namespace rtg {

namespace builtin {

struct literal
{
    std::string name() const { return "@literal"; }
    shape compute_shape(std::vector<shape>) const { RTG_THROW("builtin"); }
    argument compute(std::vector<argument>) const { RTG_THROW("builtin"); }
};

struct param
{
    std::string parameter;
    std::string name() const { return "@param"; }
    shape compute_shape(std::vector<shape>) const { RTG_THROW("builtin"); }
    argument compute(std::vector<argument>) const { RTG_THROW("builtin"); }
    friend std::ostream& operator<<(std::ostream& os, const param& op)
    {
        os << op.name() << ":" << op.parameter;
        return os;
    }
};

} // namespace builtin

} // namespace rtg

#endif
