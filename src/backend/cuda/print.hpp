#include <af/array.h>
#include <iosfwd>

namespace cuda
{
    std::ostream&
    operator<<(std::ostream &out, const cfloat& var);

    std::ostream&
    operator<<(std::ostream &out, const cdouble& var);

    template<typename T>
    void
    print(const af_array &arr);
}
