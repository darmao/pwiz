//
// PeakelGrower.cpp
//
//
// Original author: Darren Kessner <darren@proteowizard.org>
//
// Copyright 2009 Center for Applied Molecular Medicine
//   University of Southern California, Los Angeles, CA
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and 
// limitations under the License.
//
                                                                                                     
#define PWIZ_SOURCE
#include "PeakelGrower.hpp"


namespace pwiz {
namespace analysis {


using namespace std;
using namespace pwiz::data::peakdata;


//
// PeakelField
// 


namespace {
bool isWithinRetentionTimeTolerance(const Peakel& peakel, double retentionTime, 
                                    double toleranceRetentionTime)
{
    double rtLow = peakel.retentionTime;
    double rtHigh = peakel.retentionTime;

    if (!peakel.peaks.empty())
    {
        // get retentionTime range from peaks;
        // assume Peakel::peaks vector is sorted by retentionTime
        rtLow = peakel.peaks.front().retentionTime;
        rtHigh = peakel.peaks.back().retentionTime;
    }

    return (retentionTime > rtLow - toleranceRetentionTime &&
            retentionTime < rtHigh + toleranceRetentionTime);
}
} // namespace


vector<PeakelPtr> PeakelField::find(double mz, double toleranceMZ,
                                    double retentionTime, double toleranceRetentionTime) const
{
    PeakelPtr target(new Peakel);

    target->mz = mz - toleranceMZ;
    PeakelField::const_iterator begin = this->lower_bound(target);

    target->mz = mz + toleranceMZ;
    PeakelField::const_iterator end = this->upper_bound(target);

    vector<PeakelPtr> result;

    for (PeakelField::const_iterator it=begin; it!=end; ++it)
        if (isWithinRetentionTimeTolerance(**it, retentionTime, toleranceRetentionTime))
            result.push_back(*it);

    return result;
}


//
// PeakelGrower
// 


void PeakelGrower::sowPeaks(PeakelField& peakelField, const vector<Peak>& peaks) const
{
    for (vector<Peak>::const_iterator it=peaks.begin(); it!= peaks.end(); ++it)
        sowPeak(peakelField, *it);
}


void PeakelGrower::sowPeaks(PeakelField& peakelField, const vector< vector<Peak> >& peaks) const
{
    for (vector< vector<Peak> >::const_iterator it=peaks.begin(); it!= peaks.end(); ++it)
        sowPeaks(peakelField, *it);
}


//
// PeakelGrower_Proximity
// 


PeakelGrower_Proximity::PeakelGrower_Proximity(const Config& config)
:   config_(config)
{}


namespace {

//#define PEAKELGROWER_DEBUG

void insertNewPeakel(PeakelField& peakelField, const Peak& peak)
{
    PeakelPtr peakel(new Peakel);
    peakel->mz = peak.mz;
    peakel->retentionTime = peak.retentionTime;
    peakel->peaks.push_back(peak);

    peakelField.insert(peakel);

#ifdef PEAKELGROWER_DEBUG
    cout << "insertNewPeakel():\n  " << *peakel << endl;
#endif
}


void updatePeakel(Peakel& peakel, const Peak& peak)
{
    peakel.peaks.push_back(peak);

#ifdef PEAKELGROWER_DEBUG
    cout << "updatePeakel():\n  " << peakel << endl;
#endif
}

} // namespace


void PeakelGrower_Proximity::sowPeak(PeakelField& peakelField, const Peak& peak) const
{
    vector<PeakelPtr> candidates = peakelField.find(peak.mz, config_.toleranceMZ, 
                                                    peak.retentionTime, config_.toleranceRetentionTime);

    if (candidates.empty())
        insertNewPeakel(peakelField, peak);
    else if (candidates.size() == 1)
        updatePeakel(*candidates.front(), peak);
    else
    {
        cerr << "[PeakelGrower_Proximity::sowPeak()] Warning: multiple candidate peakels.\n";
        cerr << "  peak: " << peak << endl;
        cerr << "  candidates: " << candidates.size() << endl;
        for (vector<PeakelPtr>::const_iterator it=candidates.begin(); it!=candidates.end(); ++it)
            cerr << **it << endl;
        cerr << endl;
    }
}


} // namespace analysis
} // namespace pwiz


