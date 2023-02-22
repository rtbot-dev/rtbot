#include "rtbot/FactoryOp.h"
#include "rtbot/Operator.h"
#include "rtbot/tools/MovingAverage.h"
#include "rtbot/tools/PeakDetector.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"

#include <iostream>
#include <nlohmann/json.hpp>


namespace nlohmann {

template <>
struct adl_serializer<rtbot::MovingAverage> {
    static rtbot::MovingAverage from_json(const json& j) {
        std::string id = j.at("id");
        if (j.contains("coeff")) {
            std::vector<double> coeff=j.at("coeff");
            return {id, coeff};
        }
        else {
            int n=j.at("n");
            return  {id, n};
        }
    }

    static void to_json(json& j, rtbot::MovingAverage t) {
        j = t.coeff;
    }
};

template <>
struct adl_serializer<rtbot::Output<>> {
    static rtbot::Output<> from_json(const json& j) {
        std::string id = j.at("id");
        return {id};
    }

    static void to_json(json& j, rtbot::Output<> t) {
        j = t.id;
    }
};

}


namespace rtbot {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Input<double>,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeakDetector,id,n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Join<double>,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Difference,id);



template<class T>
std::unique_ptr<T> make_unique(T &&x) { return std::unique_ptr<T>(new T(std::move(x))); } // remove this if std >= c++14

Op_ptr<> FactoryOp::createOp(const std::string &json_string)
{
    auto json=nlohmann::json::parse(json_string);
    const string type=json.at("type");
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
        return make_unique(Output<double>(json.at("id")));
    else
        throw std::invalid_argument("FactoryOp::createOp unknow operator type");
}


std::string FactoryOp::createPipeline(std::string const& id, std::string const&  json_program)
{
    try
    {
        pipelines.emplace(id, createPipeline(json_program));
        return "";
    }
    catch (const nlohmann::json::parse_error& e)
    {
        // output exception information
        std::cout << "message: " << e.what() << '\n'
                  << "exception id: " << e.id << '\n'
                  << "byte position of error: " << e.byte << std::endl;
        return std::string("Unable to parse program: ") + e.what();
    }
}


///----------- the new factory ----------------


template<>
map<string, function<Op_ptr<>(string)>> Operator<double>::opFactory= {}; // TODO: update to singleton

template<>
Op_ptr<> Operator<>::parse(string const& program)
{
    auto json=nlohmann::json::parse(program);
    string type=json.at("type");
    auto it = opFactory.find(type);
    if (it == opFactory.end())
        throw std::runtime_error(string("invalid Operator type while parsin") + type );
    return it->second(program);
}


template<class T>                               // TODO: read about json::get_ptr()
unique_ptr<T> read_json(string const& prog)
{
    return std::unique_ptr<T>(new T(nlohmann::json::parse(prog).get<T>()) );
}

/// register some the operators. Notice that this can be done on any constructor
/// whenever we create a static instance later, as  below:
FactoryOp::FactoryOp()
{
    Operator<>::opFactory["Input"]=read_json<Input<>>;
    Operator<>::opFactory["MovingAverage"]=read_json<MovingAverage>;
    Operator<>::opFactory["PeakDetector"]=read_json<PeakDetector>;
    Operator<>::opFactory["Join"]=read_json<Join<>>;
    Operator<>::opFactory["Difference"]=read_json<Difference>;
    Operator<>::opFactory["Output"]=read_json<Output<>>;
}

static FactoryOp factory;






}


