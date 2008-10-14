//
// Chemistry.cpp 
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


#define PWIZ_SOURCE

#include "Chemistry.hpp"
#include "ChemistryData.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <iterator>
#include <sstream>
#include <numeric>
#include <algorithm>
#include "utility/misc/String.hpp"
#include "boost/thread/tss.hpp"

//extern "C" void tss_cleanup_implemented() {} // workaround for TSS linker error?

using namespace std;


namespace pwiz {
namespace proteome {
namespace Chemistry { 


PWIZ_API_DECL bool MassAbundance::operator==(const MassAbundance& that) const
{
    return this->mass==that.mass && this->abundance==that.abundance;
}


PWIZ_API_DECL bool MassAbundance::operator!=(const MassAbundance& that) const
{
    return !operator==(that); 
}


PWIZ_API_DECL ostream& operator<<(ostream& os, const MassAbundance& ma)
{
    os << "<" << ma.mass << ", " << ma.abundance << ">";
    return os;
}


PWIZ_API_DECL ostream& operator<<(ostream& os, const MassDistribution& md)
{
    copy(md.begin(), md.end(), ostream_iterator<MassAbundance>(os, "\n"));
    return os;
}


namespace Element {


PWIZ_API_DECL std::ostream& operator<<(std::ostream& os, Type type)
{
    Info info;
    os << info[type].symbol;
    return os;
}


class Info::Impl
{
    public:
    Impl();
    const Info::Record& record(Type type) const;

    private:
    vector<Record> data_;
    void initializeData();
};


Info::Impl::Impl() 
{
    //cout << "[Chemistry::Element::Info::initialize()]" << endl;
    initializeData();
}

const Info::Record& Info::Impl::record(Type type) const
{
    if (data_.size() <= size_t(type))
        throw runtime_error("[Chemistry::Element::Info::Impl::record()]  Record not found.");

    return data_[type];
}

void Info::Impl::initializeData()
{
    // iterate through the ChemistryData array and put it in our own data structure
    ChemistryData::Element* it = ChemistryData::elements();
    ChemistryData::Element* end = ChemistryData::elements() + ChemistryData::elementsSize();

    data_.resize(ChemistryData::elementsSize());

    for (; it!=end; ++it)
    {
        Info::Record record;
        record.type = it->type;
        record.symbol = it->symbol;
        record.atomicNumber = it->atomicNumber;
        record.atomicWeight = it->atomicWeight;
        
        for (ChemistryData::Isotope* p=it->isotopes; p<it->isotopes+it->isotopesSize; ++p)
            record.isotopes.push_back(MassAbundance(p->mass, p->abundance));        

        data_[it->type] = record;        
    }
}


// Info implementation

PWIZ_API_DECL Info::Info() : impl_(new Impl) {}
PWIZ_API_DECL Info::Info(boost::restricted) : impl_(new Impl) {}
PWIZ_API_DECL Info::~Info() {} // auto destruction of impl_
PWIZ_API_DECL const Info::Record& Info::operator[](Type type) const {return impl_->record(type);}


PWIZ_API_DECL ostream& operator<<(ostream& os, const Info::Record& r)
{
    cout << r.symbol << " " << r.atomicNumber << " " << r.atomicWeight << " ";
    copy(r.isotopes.begin(), r.isotopes.end(), ostream_iterator<MassAbundance>(cout, " "));
    return os;
}


} // namespace Element


// implementation of element symbol->type (text->enum) mapping

namespace { 

map<string, Element::Type> mapTextEnum_;

void initializeTextEnumMap()
{
    for (ChemistryData::Element* it = ChemistryData::elements(); 
         it != ChemistryData::elements() + ChemistryData::elementsSize();
         ++it)
        mapTextEnum_[it->symbol] = it->type;
}

Element::Type text2enum(const string& text)
{
    if (mapTextEnum_.empty())
        initializeTextEnumMap();
    
    if (!mapTextEnum_.count(text))
        throw runtime_error(("[Chemistry::text2enum()] Error translating symbol " + text).c_str());

    return mapTextEnum_[text];
}

} // namespace


// Formula implementation

class Formula::Impl
{
    public:

    Impl(const string& formula);

    inline void calculateMasses()
    {
        if (dirty)
        {
            Element::Info::lease info;
            dirty = false;

            monoMass = avgMass = 0;
            for (size_t i=0; i <= size_t(Element::P); ++i)
            {
                int count = CHONSP_data[i];
                if (count != 0)
                {
                    const Element::Info::Record& r = info->operator[](Element::Type(i));
                    if (!r.isotopes.empty())
                        monoMass += r.isotopes[0].mass * count;
                    avgMass += r.atomicWeight * count;
                }
            }

            for (Data::const_iterator it=data.begin(); it!=data.end(); ++it)
            {
                const Element::Info::Record& r = info->operator[](it->first);
                if (!r.isotopes.empty())
                    monoMass += r.isotopes[0].mass * it->second;
                avgMass += r.atomicWeight * it->second;
            }
        }
    }

    typedef map<Element::Type, int> Data;
    Data data;
    vector<int> CHONSP_data;
    double monoMass;
    double avgMass;
    bool dirty; // true if masses need updating
};


Formula::Impl::Impl(const string& formula)
:   monoMass(0), avgMass(0), dirty(false)
{
    CHONSP_data.resize(size_t(Element::P)+1, 0);

    if (formula.empty())
        return;

    // parse the formula string

    // this implementation is correct, but should be done with a
    // regular expression library if performance becomes an issue

    const string& whitespace_ = " \t\n\r";
    const string& digits_ = "-0123456789";
    const string& letters_ = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    Element::Info::lease info;

    string::size_type index = 0;
    while (index < formula.size())
    {
        string::size_type indexTypeBegin = formula.find_first_of(letters_, index);
        string::size_type indexTypeEnd = formula.find_first_not_of(letters_, indexTypeBegin);
        string::size_type indexCountBegin = formula.find_first_of(digits_, indexTypeEnd);
        string::size_type indexCountEnd = formula.find_first_not_of(digits_, indexCountBegin);

        if (indexTypeBegin==string::npos || indexCountBegin==string::npos) 
            throw runtime_error("[Formula::Impl::Impl()] Invalid formula: " + formula);

        string symbol = formula.substr(indexTypeBegin, indexTypeEnd-indexTypeBegin);
        int count;
        try
        {
            count = lexical_cast<int>(formula.substr(indexCountBegin, indexCountEnd-indexCountBegin));
        }
        catch(bad_lexical_cast&)
        {
            throw runtime_error("[Formula::Impl::Impl()] Invalid count in formula: " + formula);
        }

        Element::Type type = text2enum(symbol);
        if (type > Element::P)
            data[type] = count;
        else
            CHONSP_data[size_t(type)] = count;

        index = formula.find_first_not_of(whitespace_, indexCountEnd);

        const Element::Info::Record& r = info->operator[](type);
        if (!r.isotopes.empty())
            monoMass += r.isotopes[0].mass * count;
        avgMass += r.atomicWeight * count;
    }
}


PWIZ_API_DECL Formula::Formula(const string& formula)
:   impl_(new Impl(formula))
{}

PWIZ_API_DECL Formula::Formula(const char* formula)
:   impl_(new Impl(formula))
{}

PWIZ_API_DECL Formula::Formula(const Formula& formula)
:   impl_(new Impl(*formula.impl_))
{}


PWIZ_API_DECL const Formula& Formula::operator=(const Formula& formula)
{
    *impl_ = *formula.impl_;
    return *this;
}


PWIZ_API_DECL Formula::~Formula()
{} // auto destruction of impl_


PWIZ_API_DECL double Formula::monoisotopicMass() const
{
    impl_->calculateMasses();
    return impl_->monoMass;
}


PWIZ_API_DECL double Formula::molecularWeight() const
{
    impl_->calculateMasses();
    return impl_->avgMass;
}


PWIZ_API_DECL string Formula::formula() const
{
    // collect a term for each element
    vector<string> terms;

    for (size_t i=0; i <= size_t(Element::P); ++i)
    {
        int count = impl_->CHONSP_data[i];
        ostringstream term;
        if (count != 0)
            term << Element::Type(i) << count;
        terms.push_back(term.str());
    }

    for (Impl::Data::const_iterator it=impl_->data.begin(); it!=impl_->data.end(); ++it)
    { 
        ostringstream term;
        if (it->second != 0)
            term << it->first << it->second;
        terms.push_back(term.str());
    }

    // sort alphabetically and return the concatenation
    sort(terms.begin(), terms.end());
    return accumulate(terms.begin(), terms.end(), string());
}


PWIZ_API_DECL int Formula::operator[](Element::Type e) const
{
    if (e > Element::P)
        return impl_->data[e];
    else
        return impl_->CHONSP_data[e];
}


PWIZ_API_DECL int& Formula::operator[](Element::Type e)
{
    impl_->dirty = true; // worst-case
    if (e > Element::P)
        return impl_->data[e];
    else
        return impl_->CHONSP_data[e];
}


PWIZ_API_DECL map<Element::Type, int> Formula::data() const
{
    map<Element::Type, int> dataCopy(impl_->data);
    for (size_t i=0; i <= size_t(Element::P); ++i)
    {
        int count = impl_->CHONSP_data[i];
        if (count != 0)
            dataCopy[Element::Type(i)] = count;
    }
    return dataCopy;
}


PWIZ_API_DECL Formula& Formula::operator+=(const Formula& that)
{
    impl_->CHONSP_data[0] += that.impl_->CHONSP_data[0];
    impl_->CHONSP_data[1] += that.impl_->CHONSP_data[1];
    impl_->CHONSP_data[2] += that.impl_->CHONSP_data[2];
    impl_->CHONSP_data[3] += that.impl_->CHONSP_data[3];
    impl_->CHONSP_data[4] += that.impl_->CHONSP_data[4];
    impl_->CHONSP_data[5] += that.impl_->CHONSP_data[5];

    for (Map::const_iterator it=that.impl_->data.begin(); it!=that.impl_->data.end(); ++it)
        impl_->data[it->first] += it->second;
    impl_->dirty = true;
    return *this;
}


PWIZ_API_DECL Formula& Formula::operator-=(const Formula& that)
{
    impl_->CHONSP_data[0] -= that.impl_->CHONSP_data[0];
    impl_->CHONSP_data[1] -= that.impl_->CHONSP_data[1];
    impl_->CHONSP_data[2] -= that.impl_->CHONSP_data[2];
    impl_->CHONSP_data[3] -= that.impl_->CHONSP_data[3];
    impl_->CHONSP_data[4] -= that.impl_->CHONSP_data[4];
    impl_->CHONSP_data[5] -= that.impl_->CHONSP_data[5];

    for (Map::const_iterator it=that.impl_->data.begin(); it!=that.impl_->data.end(); ++it)
        impl_->data[it->first] -= it->second;
    impl_->dirty = true;
    return *this;
}


PWIZ_API_DECL Formula& Formula::operator*=(int scalar)
{
    impl_->CHONSP_data[0] *= scalar;
    impl_->CHONSP_data[1] *= scalar;
    impl_->CHONSP_data[2] *= scalar;
    impl_->CHONSP_data[3] *= scalar;
    impl_->CHONSP_data[4] *= scalar;
    impl_->CHONSP_data[5] *= scalar;

    for (Map::iterator it=impl_->data.begin(); it!=impl_->data.end(); ++it)
        it->second *= scalar;
    impl_->dirty = true;
    return *this;
}


PWIZ_API_DECL Formula operator+(const Formula& a, const Formula& b)
{
    Formula result(a);
    result += b;
    return result;
}


PWIZ_API_DECL Formula operator-(const Formula& a, const Formula& b)
{
    Formula result(a);
    result -= b;
    return result;
}


PWIZ_API_DECL Formula operator*(const Formula& a, int scalar)
{
    Formula result(a);
    result *= scalar;
    return result;
}


PWIZ_API_DECL Formula operator*(int scalar, const Formula& a)
{
    Formula result(a);
    result *= scalar;
    return result;
}


PWIZ_API_DECL ostream& operator<<(ostream& os, const Formula& formula)
{
    os << formula.formula();
    return os;
}


} // namespace Chemistry
} // namespace proteome 
} // namespace pwiz


