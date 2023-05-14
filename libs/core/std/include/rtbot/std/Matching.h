#ifndef MATCHING_H
#define MATCHING_H

#include<vector>
#include <functional>

namespace rtbot {


template<class T>
class MatrixData {
    std::vector<T> data;
public:
    int n_rows, n_cols;
    MatrixData(int n_rows_, int n_cols_)
        : data(n_rows_*n_cols_)
        , n_rows(n_rows_)
        , n_cols(n_cols_)
    {}

    T& operator()(int i,int j) { return data.at(i+j*n_rows); }
    const T& operator()(int i,int j) const { return data.at(i+j*n_rows); }
};


/**
 * Find the best matching of two lists.
 * @see https://en.wikipedia.org/wiki/Dynamic_time_warping
 * @see https://en.wikipedia.org/wiki/Needleman%E2%80%93Wunsch_algorithm
 */
template<typename T>
class Matching
{
public:
    std::vector<T> matchA;
    std::vector<T> matchB;
    std::vector<T> extraA;
    std::vector<T> extraB;

    /**
    * Compute the match between the lists a and b, where a is the reference.
    * @param a_cost is the cost for an unmatching element in a.
    * @param b_cost is the cost for an unmatching element in b.
    * @param match_condition(x,y) when not empy says if the elemements x and y can be used as match.
    */
    Matching(std::vector<T> const& a_, std::vector<T> const& b_,
                    double a_cost_, double b_cost_,
                    std::function<bool(T,T)> match_condition_={})
        : a(a_)
        , b(b_)
        , aCost(a_cost_)
        , bCost(b_cost_)
        , match_condition(match_condition_)
        , fm(a_.size()+1,b_.size()+1)
    {
        for(auto i=0u;i<=a.size();i++)
            for(auto j=0u;j<=b.size();j++)
                fm(i,j)=i*aCost+j*bCost;
        minimizeCost();
        computeMatch();
    }
    Matching(std::vector<T> const& a, std::vector<T> const& b,
                    std::function<bool(T,T)> match_condition={})
        : Matching(a,b,defaultMissingCost(a), defaultMissingCost(a), match_condition)
    {}

    /// compute the mean adjacent distance
    static double defaultMissingCost(std::vector<T> const& a) { return (a.size()>1) ? (a.back()-a.front())/(a.size()-1.0) : 1; }

    double fScore() const {return 2.0*matchA.size()/(2*matchA.size()+extraA.size()+extraB.size()); }

    double averageDistanceOfMatched() const
    {
        double sum=0.0;
        for(auto i=0u;i<matchA.size();i++)
            sum+=std::abs(matchA[i]-matchB[i]);
        return sum/matchA.size();
    }
private:
    void minimizeCost()
    {
        for (auto i=0u;i<a.size();i++)
            for (auto j=0u;j<b.size();j++) {
                auto cost = std::abs(a[i]-b[j]);
                auto matchF =
                        (!match_condition || match_condition(a[i],b[j]))
                        ? fm(i, j) + cost
                        : std::numeric_limits<double>::max();
                auto deleteF = fm(i, j + 1) + aCost;
                auto insertF = fm(i + 1, j) + bCost;
                fm(i + 1, j + 1) = std::min(std::min(matchF, deleteF), insertF);
            }
    }
    void computeMatch()
    {
        const double tol=1e-3*aCost;
        int i = a.size() - 1;
        int j = b.size() - 1;
        while (i >= 0 || j >= 0) {
            if (i >= 0 && j >= 0 &&
                std::abs( fm(i + 1, j + 1) - fm(i, j) - std::abs(a[i] - b[j]) ) < tol
            ) {
                matchA.push_back(a[i]);
                matchB.push_back(b[j]);
                i--;
                j--;
            } else if ( i >= 0 && std::abs( fm(i + 1, j + 1) - fm(i, j + 1) - aCost )<tol ) {
                extraA.push_back(a[i]);
                i--;
            } else {
                extraB.push_back(b[j]);
                j--;
            }
        }
        std::reverse(matchA.begin(),matchA.end());
        std::reverse(matchB.begin(),matchB.end());
        std::reverse(extraA.begin(),extraA.end());
        std::reverse(extraB.begin(),extraB.end());
    }

private:
    std::vector<T> a, b;
    double aCost, bCost;
    std::function<bool(T x,T y)> match_condition;
    MatrixData<double> fm;
};



}

#endif // MATCHING_H
