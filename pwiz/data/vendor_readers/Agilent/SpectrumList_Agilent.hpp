//
// SpectrumList_Agilent.hpp
//
//
// Original author: Matt Chambers <matt.chambers .@. vanderbilt.edu>
//
// Copyright 2009 Vanderbilt University - Nashville, TN 37232
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


#include "pwiz/utility/misc/Export.hpp"
#include "pwiz/data/msdata/SpectrumListBase.hpp"
#include "Reader_Agilent_Detail.hpp"
#include "pwiz/utility/misc/IntegerSet.hpp"
#include <boost/thread/once.hpp>


using namespace std;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::bad_lexical_cast;


namespace pwiz {
namespace msdata {
namespace detail {


class PWIZ_API_DECL SpectrumList_Agilent : public SpectrumListBase
{
    public:

    SpectrumList_Agilent(AgilentDataReaderPtr rawfile);
    virtual size_t size() const;
    virtual const SpectrumIdentity& spectrumIdentity(size_t index) const;
    virtual size_t find(const string& id) const;
    virtual SpectrumPtr spectrum(size_t index, bool getBinaryData) const;
    virtual SpectrumPtr spectrum(size_t index, bool getBinaryData, const pwiz::util::IntegerSet& msLevelsToCentroid) const;
    
    private:

    AgilentDataReaderPtr rawfile_;
    mutable size_t size_;

    mutable boost::once_flag indexInitialized_;

    struct IndexEntry : public SpectrumIdentity
    {
        long scan; // unique but not contiguous
    };

    mutable vector<IndexEntry> index_;
    mutable map<string, size_t> idToIndexMap_;

    void createIndex() const;
};


} // detail
} // msdata
} // pwiz

