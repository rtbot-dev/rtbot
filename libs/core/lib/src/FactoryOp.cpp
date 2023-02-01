#include "rtbot/FactoryOp.h"
#include "rtbot/Operator.h"
#include "rtbot/MovingAverage.h"
#include "rtbot/PeakDetector.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"

#include <iostream>
#include <nlohmann/json.hpp>


namespace rtbot {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Input<double>,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MovingAverage,id,n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeakDetector,id,n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Join<double>,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Difference,id);

template<class T>
std::unique_ptr<T> make_unique(T &&x) { return std::unique_ptr<T>(new T(std::move(x))); } // remove this if std >= c++14

Op_ptr FactoryOp::createOp(const char json_string[])
{
    auto json=nlohmann::json::parse(json_string);
    const string type=json["type"];
    if (type=="Input")
        return make_unique(json.get<Input<double>>());
    else if (type=="MovingAverage")
        return make_unique(json.get<MovingAverage>());
    else if (type=="PeakDetector")
        return make_unique(json.get<PeakDetector>());
    else if (type=="Join")
        return make_unique(json.get<Join<double>>());
    else if (type=="Difference")
        return make_unique(json.get<Difference>());
    else if (type=="Output")
        return make_unique(Output<double>(json["id"]));
    else
        throw std::invalid_argument("FactoryOp::createOp unknow operator type");
}


}


