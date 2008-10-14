//
// AminoAcidTest.cpp
//
//
// Original author: Darren Kessner <Darren.Kessner@cshs.org>
//
// Copyright 2006 Louis Warschaw Prostate Cancer Center
//   Cedars Sinai Medical Center, Los Angeles, California  90048
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


#include "AminoAcid.hpp"
#include "utility/misc/unit.hpp"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include "boost/thread/thread.hpp"
#include "boost/thread/barrier.hpp"


using namespace std;
using namespace pwiz::util;
using namespace pwiz::proteome;
using namespace pwiz::proteome::Chemistry;
using namespace pwiz::proteome::Chemistry::Element;
using namespace pwiz::proteome::AminoAcid;


ostream* os_ = 0;


bool hasLowerMass(const AminoAcid::Info::Record& a, const AminoAcid::Info::Record& b)
{
    return a.formula.monoisotopicMass() < b.formula.monoisotopicMass();
}


void printRecord(ostream* os, const AminoAcid::Info::Record& record)
{
    if (!os) return;

    *os << record.symbol << ": " 
        << setw(14) << record.name << " " 
        << setw(11) << record.formula << " " 
        << setprecision(3)
        << setw(5) << record.abundance << " "
        << fixed << setprecision(6)
        << setw(10) << record.formula.monoisotopicMass() << " "
        << setw(10) << record.formula.molecularWeight() << " "
        << setw(10) << record.residueFormula.monoisotopicMass() << " "
        << setw(10) << record.residueFormula.molecularWeight() << endl;
}


struct TestAminoAcid
{
    double monoMass;
    double avgMass;
    char symbol;
};

// masses copied from http://www.unimod.org/xml/unimod.xml
TestAminoAcid testAminoAcids[] =
{
    { 71.0371140, 71.07790, 'A' },    // Alanine
    { 156.101111, 156.1857, 'R' },    // Arginine
    { 114.042927, 114.1026, 'N' },    // Asparagine
    { 115.026943, 115.0874, 'D' },    // Aspartic acid
    { 103.009185, 103.1429, 'C' },    // Cysteine
    { 129.042593, 129.1140, 'E' },    // Glutamic acid
    { 128.058578, 128.1292, 'Q' },    // Glutamine
    { 57.0214640, 57.05130, 'G' },    // Glycine
    { 137.058912, 137.1393, 'H' },    // Histidine
    { 113.084064, 113.1576, 'I' },    // Isoleucine
    { 113.084064, 113.1576, 'L' },    // Leucine
    { 128.094963, 128.1723, 'K' },    // Lysine
    { 131.040485, 131.1961, 'M' },    // Methionine
    { 147.068414, 147.1739, 'F' },    // Phenylalanine
    { 97.0527640, 97.11520, 'P' },    // Proline
    { 87.0320280, 87.07730, 'S' },    // Serine
    { 101.047679, 101.1039, 'T' },    // Threonine
    { 186.079313, 186.2099, 'W' },    // Tryptophan
    { 163.063329, 163.1733, 'Y' },    // Tyrosine
    { 99.0684140, 99.13110, 'V' },    // Valine
    { 114.042927, 114.1026, 'B' },    // AspX
    { 128.058578, 128.1292, 'Z' },    // GlutX
    { 114.091900, 114.1674, 'X' }     // Unknown (Averagine)
};


void test()
{
    AminoAcid::Info info;

    // get a copy of all the records

    vector<AminoAcid::Info::Record> records;

    for (char symbol='A'; symbol<='Z'; symbol++)
    {
        try 
        {
            const AminoAcid::Info::Record& record = info[symbol];
            records.push_back(record);
        }
        catch (exception&)
        {}
    }

    for (vector<AminoAcid::Info::Record>::iterator it=records.begin(); it!=records.end(); ++it)
        printRecord(os_, *it);

    unit_assert(info[Alanine].residueFormula[C] == 3);
    unit_assert(info[Alanine].residueFormula[H] == 5);
    unit_assert(info[Alanine].residueFormula[N] == 1);
    unit_assert(info[Alanine].residueFormula[O] == 1);
    unit_assert(info[Alanine].residueFormula[S] == 0);

    unit_assert(info[Alanine].formula[C] == 3);
    unit_assert(info[Alanine].formula[H] == 7);
    unit_assert(info[Alanine].formula[N] == 1);
    unit_assert(info[Alanine].formula[O] == 2);
    unit_assert(info[Alanine].formula[S] == 0);


    // test single amino acids
    for (int i=0; i < 22; ++i) // skip X for now
    {
        TestAminoAcid& aa = testAminoAcids[i];
        Chemistry::Formula residueFormula = info[aa.symbol].residueFormula;
        unit_assert_equal(residueFormula.monoisotopicMass(), aa.monoMass, 0.00001);
        unit_assert_equal(residueFormula.molecularWeight(), aa.avgMass, 0.0001);
        //set<char> mmNames = mm2n.getNames(aa.monoMass, EPSILON);
        //set<char> amNames = am2n.getNames(aa.avgMass, EPSILON);
        //unit_assert(mmNames.count(aa.symbol) > 0);
        //unit_assert(amNames.count(aa.symbol) > 0);
    }


    // compute some averages

    double averageMonoisotopicMass = 0;
    double averageC = 0;
    double averageH = 0;
    double averageN = 0;
    double averageO = 0;
    double averageS = 0;

    for (vector<AminoAcid::Info::Record>::iterator it=records.begin(); it!=records.end(); ++it)
    {
        const AminoAcid::Info::Record& record = *it;

        Chemistry::Formula residueFormula = record.residueFormula;
        averageMonoisotopicMass += residueFormula.monoisotopicMass() * record.abundance; 
        averageC += residueFormula[C] * record.abundance;
        averageH += residueFormula[H] * record.abundance;
        averageN += residueFormula[N] * record.abundance;
        averageO += residueFormula[O] * record.abundance;
        averageS += residueFormula[S] * record.abundance;
    }

    if (os_) *os_ << setprecision(8) << endl;
    if (os_) *os_ << "average residue C: " << averageC << endl;    
    if (os_) *os_ << "average residue H: " << averageH << endl;    
    if (os_) *os_ << "average residue N: " << averageN << endl;    
    if (os_) *os_ << "average residue O: " << averageO << endl;    
    if (os_) *os_ << "average residue S: " << averageS << endl;    
    if (os_) *os_ << endl;

    if (os_) *os_ << "average monoisotopic mass: " << averageMonoisotopicMass << endl;
    double averageResidueMass = averageMonoisotopicMass;
    if (os_) *os_ << "average residue mass: " << averageResidueMass << endl << endl;

    // sort by monoisotopic mass and print again
    sort(records.begin(), records.end(), hasLowerMass);
    for (vector<AminoAcid::Info::Record>::iterator it=records.begin(); it!=records.end(); ++it)
        printRecord(os_, *it); 
}


void testThreadSafetyWorker(boost::barrier* testBarrier)
{
    testBarrier->wait(); // wait until all threads have started

    // test thread-specific singleton access with instance
    unit_assert(AminoAcid::Info::instance->operator[](Glycine).symbol == 'G');

    // test thread-specific singleton access with lease
    AminoAcid::Info::lease info;
    unit_assert(info->operator[](Glycine).symbol == 'G');
}

void testThreadSafety()
{
    const int testThreadCount = 100;
    boost::barrier testBarrier(testThreadCount);
    boost::thread_group testThreadGroup;
    for (int i=0; i < testThreadCount; ++i)
        testThreadGroup.add_thread(new boost::thread(&testThreadSafetyWorker, &testBarrier));
    testThreadGroup.join_all();
}


int main(int argc, char* argv[])
{
    try
    {
        if (argc>1 && !strcmp(argv[1],"-v")) os_ = &cout;
        if (os_) *os_ << "AminoAcidTest\n";
        test();
        testThreadSafety();
        return 0;
    }
    catch (exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
}


