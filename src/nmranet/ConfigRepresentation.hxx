/** \copyright
 * Copyright (c) 2015, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file ConfigRepresentation.hxx
 *
 * Static representation of a config file.
 *
 * @author Balazs Racz
 * @date 31 May 2014
 */

#ifndef _NMRANET_CONFIGREPRESENTATION_HXX_
#define _NMRANET_CONFIGREPRESENTATION_HXX_

#include "nmranet/ConfigEntry.hxx"
#include "nmranet/MemoryConfig.hxx"

namespace nmranet
{

class GroupBaseEntry : public nmranet::ConfigReference
{
public:
    INHERIT_CONSTEXPR_CONSTRUCTOR(GroupBaseEntry, ConfigReference)
    static constexpr unsigned size()
    {
        return 0;
    }
    constexpr unsigned end_offset()
    {
        return offset_;
    }
};

template <int N> class EntryMarker
{
public:
    constexpr EntryMarker()
    {
    }
};

class NoopGroupEntry : public ConfigReference
{
public:
    INHERIT_CONSTEXPR_CONSTRUCTOR(NoopGroupEntry, ConfigReference);
    constexpr unsigned end_offset()
    {
        return offset_;
    }
    static constexpr unsigned size()
    {
        return 0;
    }
};

class GroupBase : public nmranet::ConfigReference
{
public:
    INHERIT_CONSTEXPR_CONSTRUCTOR(GroupBase, ConfigReference)
    static constexpr GroupConfigOptions group_opts()
    {
        return GroupConfigOptions();
    }
    using Name = AtomConfigOptions::Name;
    using Description = AtomConfigOptions::Description;
    using Segment = GroupConfigOptions::Segment;
    using Offset = GroupConfigOptions::Offset;
    using MainCdi = GroupConfigOptions::MainCdi;
};

#define CDI_GROUP_HELPER(START_LINE, GroupName, ARGS...)                       \
    struct GroupName : public nmranet::GroupBase                               \
    {                                                                          \
        INHERIT_CONSTEXPR_CONSTRUCTOR(GroupName, GroupBase);                   \
        constexpr GroupBaseEntry entry(const EntryMarker<START_LINE> &)        \
        {                                                                      \
            return GroupBaseEntry(offset_);                                    \
        }                                                                      \
        static constexpr GroupConfigOptions group_opts()                       \
        {                                                                      \
            return GroupConfigOptions(ARGS);                                   \
        }                                                                      \
        static constexpr unsigned size()                                       \
        {                                                                      \
            return GroupName(0).end_offset();                                  \
        }                                                                      \
        template <int LINE>                                                    \
        constexpr NoopGroupEntry entry(const EntryMarker<LINE> &)              \
        {                                                                      \
            return NoopGroupEntry(                                             \
                entry(EntryMarker<LINE - 1>()).end_offset());                  \
        }                                                                      \
        template <int LINE>                                                    \
        static void render_content_cdi(                                        \
            const EntryMarker<LINE> &, std::string *s)                         \
        {                                                                      \
            render_content_cdi(EntryMarker<LINE - 1>(), s);                    \
        }                                                                      \
        static void render_content_cdi(                                        \
            const EntryMarker<START_LINE> &, std::string *s)                   \
        {                                                                      \
        }                                                                      \
        static constexpr GroupConfigRenderer<GroupName> config_renderer()      \
        {                                                                      \
            return GroupConfigRenderer<GroupName>(1, GroupName(0));            \
        }

/// @todo (balazs.racz) the group config renderer should not get an instance of
/// the current group.

#define CDI_GROUP_ENTRY_HELPER(LINE, NAME, TYPE, ...)                          \
    constexpr TYPE entry(const EntryMarker<LINE> &)                            \
    {                                                                          \
        static_assert(                                                         \
            !group_opts().is_cdi() || TYPE(0).group_opts().is_segment(),       \
            "May only have segments inside CDI.");                             \
        return TYPE(group_opts().is_cdi()                                      \
                ? TYPE(0).group_opts().get_segment_offset()                    \
                : entry(EntryMarker<LINE - 1>()).end_offset());                \
    }                                                                          \
    constexpr TYPE NAME()                                                      \
    {                                                                          \
        return entry(EntryMarker<LINE>());                                     \
    }                                                                          \
    static void render_content_cdi(const EntryMarker<LINE> &, std::string *s)  \
    {                                                                          \
        render_content_cdi(EntryMarker<LINE - 1>(), s);                        \
        TYPE::config_renderer().render_cdi(s, ##__VA_ARGS__);                  \
    }

#define CDI_GROUP_END_HELPER(LINE)                                             \
    constexpr unsigned end_offset()                                            \
    {                                                                          \
        return entry(EntryMarker<LINE>()).end_offset();                        \
    }                                                                          \
    static void render_content_cdi(std::string *s)                             \
    {                                                                          \
        return render_content_cdi(EntryMarker<LINE>(), s);                     \
    }                                                                          \
    }

/// Starts a CDI group.
///
/// @param group is the c++ name of the struct that is being defined.
/// @param ARGS are additional arguments for group options, like Name(...),
/// Description(...), Segment(...), Offset(...) or MainCdi().
#define CDI_GROUP(GroupName, ARGS...)                                          \
    CDI_GROUP_HELPER(__LINE__, GroupName, ##ARGS)

/// Adds an entry to a CDI group.
///
/// @param NAME is the c++ name of the entry
/// @param TYPE is the c++ class / struct of the entry being added
/// @param ARGS are additional arguments for the entry options, like Name(...),
/// Description(...). If a subgroup is added, then group options are also
/// allowed and they will override the respective values from the group
/// definition.
#define CDI_GROUP_ENTRY(NAME, TYPE, ...)                                       \
    CDI_GROUP_ENTRY_HELPER(__LINE__, NAME, TYPE, ##__VA_ARGS__)

/// Closes a CDI group structure definition.
#define CDI_GROUP_END() CDI_GROUP_END_HELPER(__LINE__)

/// Defines a repeated group of a given type and a given number of repeats.
///
/// Typical usage:
///
///  using AllConsumers = RepeatedGroup<ConsumerConfig, 3>;
///
/// then add AllConsumers as an entry to the enclosing group or segment.
template <class Group, unsigned N> class RepeatedGroup : public ConfigEntryBase
{
public:
    using base_type = ConfigEntryBase;
    INHERIT_CONSTEXPR_CONSTRUCTOR(RepeatedGroup, base_type)
    static constexpr unsigned size()
    {
        return Group::size() * N;
    }
    constexpr unsigned end_offset()
    {
        return offset() + size();
    }
    template <int K> constexpr Group entry()
    {
        static_assert(K < N, "Tried to fetch an entry of a repeated "
                             "group that does not exist!");
        return Group(offset_ + (K * Group::size()));
    }

    Group entry(unsigned k)
    {
        HASSERT(k < N);
        return Group(offset_ + (k * Group::size()));
    }

    static constexpr GroupConfigRenderer<Group> config_renderer()
    {
        /// @todo (balazs.racz) get rid of the instance of Group here.
        return GroupConfigRenderer<Group>(N, Group(0));
    }
};

///
/// Defines an empty group with no members, but blocking a certain amount of
/// space in the rendered configuration.
///
template <unsigned N> class EmptyGroup : public ConfigEntryBase
{
public:
    using base_type = ConfigEntryBase;
    INHERIT_CONSTEXPR_CONSTRUCTOR(EmptyGroup, base_type)
    static constexpr unsigned size()
    {
        return N;
    }
    constexpr unsigned end_offset()
    {
        return offset() + size();
    }
    static constexpr EmptyGroupConfigRenderer config_renderer()
    {
        return EmptyGroupConfigRenderer(N);
    }
};

/// Base class for all entries that can appear in the MainCdi group. THe common
/// property of these entries is that they do not rely on the offset/size
/// propagation of the previous entries, because they either do not take part
/// in the layout algorithm (e.g. the <identification> tag) or they specify the
/// origin explcitly.
class ToplevelEntryBase : public ConfigEntryBase
{
public:
    using base_type = ConfigEntryBase;
    INHERIT_CONSTEXPR_CONSTRUCTOR(ToplevelEntryBase, base_type)
    static constexpr GroupConfigOptions group_opts()
    {
        return GroupConfigOptions(GroupConfigOptions::Segment(1000));
    }
    static constexpr unsigned size()
    {
        return 0;
    }
    constexpr unsigned end_offset()
    {
        return offset() + size();
    }
};

/// Add this entry to the beginning of the CDI group to render an
/// <identification> tag at the beginning of the output cdi.xml. Requires a
/// global symbol of @ref nmranet::SNIP_STATIC_DATA to fill in the specific
/// values of the identification tree.
class Identification : public ToplevelEntryBase
{
public:
    using base_type = ToplevelEntryBase;
    INHERIT_CONSTEXPR_CONSTRUCTOR(Identification, base_type)
    static constexpr IdentificationRenderer config_renderer()
    {
        return IdentificationRenderer();
    }
};

/// Renders an <acdi> tag in the CDI group.
class Acdi : public ToplevelEntryBase
{
public:
    using base_type = ToplevelEntryBase;
    INHERIT_CONSTEXPR_CONSTRUCTOR(Acdi, base_type)
    static constexpr AcdiRenderer config_renderer()
    {
        return AcdiRenderer();
    }
};

/// Configuration description for a segment containing the ACDI user-modifiable
/// data. The implementation refers to the ACDI-userdata space number and does
/// not depend on where the actual data is located.
CDI_GROUP(
    UserInfoSegment, Segment(MemoryConfigDefs::SPACE_ACDI_USR), Offset(1));
CDI_GROUP_ENTRY(
    name, StringConfigEntry<63>, //
    Name("User name"),           //
    Description(
        "This name will appear in network browsers for the current node."));
CDI_GROUP_ENTRY(description, StringConfigEntry<64>, //
    Name("User description"),                       //
    Description("This description will appear in network browsers for the "
                "current node."));
CDI_GROUP_END();

} // namespace nmranet

#endif // _NMRANET_CONFIGREPRESENTATION_HXX_