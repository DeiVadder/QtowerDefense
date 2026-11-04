#include <QtTypes>

#include <limits>

// The overflow is the point of this program, volatile keeps the compiler from seeing it coming.
int main(int argc, char *[])
{
    const volatile qint64 number = std::numeric_limits<qint64>::max();
    const qint64 sum = number + argc;
    return sum == 0 ? 1 : 0;
}
