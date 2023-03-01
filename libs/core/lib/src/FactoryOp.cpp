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

    static void to_json(json& j, rtbot::MovingAverage const& t) {
        j = t.coeff;
    }
};

template <>
struct adl_serializer<rtbot::Output<>> {
    static rtbot::Output<> from_json(const json& j) {
        std::string id = j.at("id");
        return {id};
    }

    static void to_json(json& j, rtbot::Output<> const& t) {
        j = t.id;
    }
};

}


namespace rtbot {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Input<double>,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeakDetector,id,n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Join<double>,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Difference,id);


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


template<class Op>
FactoryOp::SerializerOp SerializerOp_json()
{
    auto read=[](string const& prog)            // TODO: read about json::get_ptr()
    {
        return std::unique_ptr<Op>(new Op(nlohmann::json::parse(prog).get<Op>()) );
    };
    auto write=[](Op_ptr<> const& op)
    {
        return nlohmann::json( *dynamic_cast<Op*>(op.get()) ).dump();
    };

    return {read,write};
};

/// register some the operators. Notice that this can be done on any constructor
/// whenever we create a static instance later, as  below:
FactoryOp::FactoryOp()
{
    op_registry().emplace("Input", SerializerOp_json<Input<>>());
    op_registry().emplace("MovingAverage", SerializerOp_json<MovingAverage>());
    op_registry().emplace("PeakDetector", SerializerOp_json<PeakDetector>());
    op_registry().emplace("Join", SerializerOp_json<Join<>>());
    op_registry().emplace("Difference", SerializerOp_json<Difference>());
    op_registry().emplace("Output", SerializerOp_json<Output<>>());
}

static FactoryOp factory;

Op_ptr<> FactoryOp::readOp(const std::string &program)
{
    auto json=nlohmann::json::parse(program);
    string type=json.at("type");
    auto it = op_registry().find(type);
    if (it == op_registry().end())
        throw std::runtime_error(string("invalid Operator type while parsing ") + type );
    return it->second.read(program);
}

std::string FactoryOp::writeOp(Op_ptr<> const& op)
{
    string type=op->typeName();
    nlohmann::json j;
    j["type"]=type;
    auto it = op_registry().find(type);
    if (it == op_registry().end())
        throw std::runtime_error(string("invalid Operator type while parsing ") + type );
    return it->second.write(op);
}







}


