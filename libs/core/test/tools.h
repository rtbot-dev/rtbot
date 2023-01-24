#ifndef TOOLS_H
#define TOOLS_H

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rtbot {

using std::vector;
using std::string;

struct SamplePPG
{
    vector<int> ti;
    vector<double> ppg;

    SamplePPG(const char filename[])
    {
        std::ifstream in(filename);
        if (!in.is_open()) throw std::runtime_error("SamplePPG: file not found");
        string line;
        getline(in,line); // drop first line
        while (getline(in,line))
        {
            auto words=split(line,',');
            ti.push_back(atof(words[0].c_str())*1000);
            ppg.push_back(-atof(words[1].c_str()));
        }
    }

    double dt() const { return (ti.back()-ti.front())/(ti.size()-1); }

private:
    static vector<string> split(string s, char delim=' ')
    {
        vector<string> out;
        std::istringstream is(s);
        string word;
        while (getline(is,word,delim))
            out.push_back(word);
        return out;
    }
};

}


#endif // TOOLS_H
