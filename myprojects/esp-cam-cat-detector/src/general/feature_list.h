//
// Created by xav on 12/7/25.
//

#ifndef FEATURE_LIST_H
#define FEATURE_LIST_H
#ifdef RAW_FEATURES_TESTING_ENABLED

#define FEATURES_SIZE 9216

constexpr const float featuresList[][FEATURES_SIZE]  = {
    {}
    // raw features from edge impulse go here
};
constexpr const char labels[][16] = {
    "empty"
    //labels go here in the same order from above
};

#endif
#endif //FEATURE_LIST_H
