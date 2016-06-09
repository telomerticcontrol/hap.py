// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Copyright (c) 2010-2015 Illumina, Inc.
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.

// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


/**
 *  \brief Variant implementation
 *
 * \file Variant.cpp
 * \author Peter Krusche
 * \email pkrusche@illumina.com
 *
 */

#include <htslib/synced_bcf_reader.h>
#include "VariantImpl.hh"

// #define DEBUG_VARIANT_GTS

namespace variant
{

    /**
     * @brief Classify a variant's GT type
     *
     */
    gttype getGTType(Call const& var)
    {
        if(var.ngt > 0)
        {
            if (var.ngt == 1)
            {
                if(var.gt[0] > 0)
                {
                    return gt_haploid;
                }
                else if(var.gt[0] == 0)
                {
                    return gt_homref;
                }
            }
            else if (var.ngt == 2)
            {
                if(var.gt[0] == 0 && var.gt[1] == 0)
                {
                    return gt_homref;
                }
                else if(    (var.gt[0] == 0 && var.gt[1] > 0)
                            || (var.gt[1] == 0 && var.gt[0] > 0)  )
                {
                    return gt_het;
                }
                else if(    var.gt[0] > 0 && var.gt[1] > 0
                            && var.gt[0] == var.gt[1] )
                {
                    return gt_homalt;
                }
                else if(    var.gt[0] > 0 && var.gt[1] > 0
                            && var.gt[0] != var.gt[1] )
                {
                    return gt_hetalt;
                }
            }
        }

        return gt_unknown;
    }

    std::ostream & operator<<(std::ostream &o, gttype const & v)
    {
        switch(v)
        {
            case gt_haploid:
                o << "gt_haploid";
                break;
            case gt_homref:
                o << "gt_homref";
                break;
            case gt_homalt:
                o << "gt_homalt";
                break;
            case gt_het:
                o << "gt_het";
                break;
            case gt_hetalt:
                o << "gt_hetalt";
                break;
            case gt_unknown:
            default:
                o << "gt_unknown";
                break;
        }

        return o;
    }

    std::ostream & operator<<(std::ostream &o, Call const & v)
    {
        if(v.ngt == 0)
        {
            o << ".";
        }
        for (size_t i = 0; i < v.ngt; ++i)
        {
            if(i > 0)
            {
                o << (v.phased ? "|" : "/");
            }
            o << v.gt[i];
        }

        if(v.nfilter > 0)
        {
            o << " ";
            for (size_t i = 0; i < v.nfilter; ++i)
            {
                if(i > 0)
                {
                    o << ",";
                }
                o << v.filter[i];
            }
        }

        return o;
    }

    std::ostream & operator<<(std::ostream &o, Variants const & v)
    {
        o << v.chr << ":" << v.pos << "-" << (v.pos + v.len - 1);

        for (RefVar const & rv : v.variation)
        {
            o << " " << rv;
        }

        for (Call const& c : v.calls)
        {
            o << " " << c;
        }

        bool any_ambiguous = false;
        for (auto & x : v.ambiguous_alleles)
        {
            if(!x.empty())
            {
                any_ambiguous = true;
                break;
            }
        }

        if (any_ambiguous)
        {
            o << "ambig[";
            for (auto & x : v.ambiguous_alleles)
            {
                for(auto y : x)
                {
                    o << y << " ";
                }
                o << ";";
            }
            o << "]";
        }
        return o;
    }

    uint64_t Variants::MAX_VID = 0;
    Variants::Variants() : id(MAX_VID++) {}

    /** interface to set / get INFO values */
    int Variants::getInfoInt(const char * id) const
    {
        for(auto const & c : calls)
        {
            int tmp = bcfhelpers::getInfoInt(c.bcf_hdr.get(), c.bcf_rec.get(), id, bcf_int32_missing);
            if(tmp != bcf_int32_missing)
            {
                return tmp;
            }
        }
        return bcf_int32_missing;
    }

    float Variants::getInfoFloat(const char * id) const
    {
        for(auto const & c : calls)
        {
            double tmp = bcfhelpers::getInfoDouble(c.bcf_hdr.get(), c.bcf_rec.get(), id);
            if(!isnan(tmp))
            {
                return (float)tmp;
            }
        }
        return std::numeric_limits<float>::quiet_NaN();
    }

    std::string Variants::getInfoString(const char * id) const
    {
        for(auto const & c : calls)
        {
            std::string tmp = bcfhelpers::getInfoString(c.bcf_hdr.get(), c.bcf_rec.get(), id, "");
            if(!tmp.empty())
            {
                return tmp;
            }
        }
        return "";
    }

    bool Variants::getInfoFlag(const char * id) const
    {
        for(auto const & c : calls)
        {
            if(bcfhelpers::getInfoFlag(c.bcf_hdr.get(), c.bcf_rec.get(), id))
            {
                return true;
            }
        }
        return false;
    }

    void Variants::delInfo(const char * id)
    {
        for(auto const & c : calls)
        {
            bcf_update_info_string(c.bcf_hdr.get(), c.bcf_rec.get(), id, NULL);
        }
    }

    void Variants::setInfo(const char * id, bool flag)
    {
        if(!flag)
        {
            delInfo(id);
        }
        else
        {
            for(auto const & c : calls)
            {
                bcf_update_info_flag(c.bcf_hdr.get(), c.bcf_rec.get(), id, NULL, 1);
            }
        }
    }

    void Variants::setInfo(const char * id, int val)
    {
        for(auto const & c : calls)
        {
            bcf_update_info_int32(c.bcf_hdr.get(), c.bcf_rec.get(), id, &val, 1);
        }
    }

    void Variants::setInfo(const char * id, float val)
    {
        for(auto const & c : calls)
        {
            bcf_update_info_float(c.bcf_hdr.get(), c.bcf_rec.get(), id, &val, 1);
        }
    }

    void Variants::setInfo(const char * id, const char * val)
    {
        for(auto const & c : calls)
        {
            bcf_update_info_string(c.bcf_hdr.get(), c.bcf_rec.get(), id, val);
        }
    }
}
