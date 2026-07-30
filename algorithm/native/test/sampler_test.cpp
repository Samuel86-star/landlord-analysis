#include "landlord.h"
#include <iostream>

using namespace landlord;

int main() {
    loadScoringConfigFromFile("config/scoring.properties");
    shuffleConfig().enabled = false;
    ShuffleDealStrategy strategy;
    DealDistributionSampler sampler(strategy);

    std::cout << "[Sampler] 开始采样 10000 局..." << std::endl;
    auto result = sampler.sample(10000);

    result.printDetailedDistribution();
    result.printRecommendedThresholds();

    std::cout << "\nSampler test completed!" << std::endl;
    return 0;
}
