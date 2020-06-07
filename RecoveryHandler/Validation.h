#include <stdint.h>

typedef struct TimeInterval
{
	uint64_t vBegin;
	uint64_t vEnd;

} TimeInterval_t;

TimeInterval_t calcValidInterval(uint8_t owner, uint8_t dataId, TimeInterval_t taskInterval);