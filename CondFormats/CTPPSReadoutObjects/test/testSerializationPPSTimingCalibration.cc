#include "CondFormats/Serialization/interface/Test.h"
#include "CondFormats/CTPPSReadoutObjects/interface/PPSTimingCalibration.h"

int main()
{
  testSerialization<PPSTimingCalibration>();
  testSerialization<PPSTimingCalibration::Key>();
  testSerialization<std::vector<PPSTimingCalibration> >();
  return 0;
}

