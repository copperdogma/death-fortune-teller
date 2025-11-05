#ifndef INFRA_RANDOM_SOURCE_H
#define INFRA_RANDOM_SOURCE_H

namespace infra {

class IRandomSource {
public:
    virtual ~IRandomSource() = default;
    virtual int nextInt(int minInclusive, int maxExclusive) = 0;
};

} // namespace infra

#endif // INFRA_RANDOM_SOURCE_H
