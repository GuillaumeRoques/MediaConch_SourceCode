/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a GPLv3+/MPLv2+ license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <iostream>
#include <sstream>
#include <string.h>
#include <ZenLib/Ztring.h>
#include <ZenLib/ZtringList.h>
#include <ZenLib/File.h>
#include "Policies.h"
#include "Core.h"
#include "Policy.h"
#include "XsltPolicy.h"
#include "UnknownPolicy.h"
//---------------------------------------------------------------------------

namespace MediaConch {

//***************************************************************************
// Policies
//***************************************************************************

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

std::map<std::string, std::list<std::string> > Policies::existing_type = std::map<std::string, std::list<std::string> >();
std::list<Policies::validatorType> Policies::existing_validator = std::list<Policies::validatorType>();
std::list<std::string> Policies::existing_xsltOperator = std::list<std::string>();
size_t Policies::policy_global_id = 0;

//---------------------------------------------------------------------------
Policies::Policies(Core *c) : core(c)
{
}

Policies::~Policies()
{
    for (size_t i = 0; i < policies.size(); ++i)
        delete policies[i];
    policies.clear();
    xmlCleanupParser();
}

// Policy
int Policies::create_xslt_policy(std::string&)
{
    Policy *p = new XsltPolicy(this, !core->accepts_https());

    // Policy filename
    find_save_name(NULL, p->filename);
    find_new_policy_name(p->name);
    size_t pos = policies.size();
    policies.push_back(p);

    return (int)pos;
}

int Policies::import_policy(const std::string& filename)
{
    if (!filename.length())
    {
        error = "The policy file does not exist";
        return -1;
    }

    std::string save_name;
    find_save_name(filename.c_str(), save_name);

    Policy *p = NULL;
    int ret = -1;
    p = new XsltPolicy(this, !core->accepts_https());
    ret = p->import_schema(filename, save_name);
    if (ret < 0)
    {
        if (p)
            delete p;
        p = new UnknownPolicy(this, !core->accepts_https());
        ret = p->import_schema(filename, save_name);
        if (ret < 0)
            error = p->get_error();
    }

    if (ret >= 0)
        policies.push_back(p);
    else
        delete p;
    return ret;
}

int Policies::import_policy_from_memory(const char* filename, const char* buffer, int len, bool is_system_policy)
{
    if (!buffer || !len)
    {
        error = "The policy does not exist";
        return -1;
    }

    std::string save_name;
    if (is_system_policy && filename)
        save_name = filename;
    else
        find_save_name(NULL, save_name);

    Policy *p = NULL;
    int ret = -1;

    p = new XsltPolicy(this, !core->accepts_https());
    ret = p->import_schema_from_memory(buffer, len, save_name);
    if (ret < 0)
    {
        if (p)
            delete p;
        p = new UnknownPolicy(this, !core->accepts_https());
        ret = p->import_schema_from_memory(buffer, len, save_name);
        if (ret < 0)
            error = p->get_error();
    }

    if (ret >= 0)
        policies.push_back(p);
    else
        delete p;
    p->is_system = is_system_policy;
    return ret;
}

int Policies::save_policy(size_t index, std::string& err)
{
    return export_policy(NULL, index, err);
}

int Policies::duplicate_policy(int id, std::string& err)
{
    if (id < 0 || id > (int)policies.size())
    {
        err = "policy id is not existing";
        return -1;
    }

    Policy *old = policies[id];
    if (!old)
    {
        err = "policy id is not existing anymore";
        return -1;
    }

    Policy *p = NULL;
    if (old->type == POLICY_XSLT)
        p = new XsltPolicy((XsltPolicy*)old);
    else if (old->type == POLICY_UNKNOWN)
        p = new UnknownPolicy((UnknownPolicy*)old);

    if (!p)
    {
        err = "policy cannot be duplicate";
        return -1;
    }

    // Policy filename
    find_save_name(NULL, p->filename);
    p->name += "_copy";
    size_t pos = policies.size();
    policies.push_back(p);

    if (old->type == POLICY_UNKNOWN)
        export_policy(p->filename.c_str(), (size_t)id, err);

    return (int)pos;
}

int Policies::export_policy(const char* filename, size_t pos, std::string& err)
{
    if ((int)pos == -1 || pos >= policies.size() || !policies[pos])
    {
        err = "Policy Id is not existing";
        return -1;
    }

    if (filename == NULL)
    {
        if (policies[pos]->is_system)
            return 0;
        filename = policies[pos]->filename.c_str();
    }

    return policies[pos]->export_schema(filename, err);
}

int Policies::dump_policy_to_memory(int pos, std::string& memory, std::string& err)
{
    if (pos == -1 || (size_t)pos >= policies.size() || !policies[pos])
    {
        err = "Policy Id is not existing";
        return -1;
    }

    if (policies[pos]->dump_schema(memory) < 0)
    {
        err = "Cannot dump the schema to memory";
        return -1;
    }

    return 0;
}

Policy* Policies::get_policy(int pos)
{
    if (pos < 0 || pos >= (int)policies.size())
        return NULL;

    return policies[pos];
}

int Policies::erase_policy(size_t index, std::string& err)
{
    if ((int)index < 0 || index >= policies.size())
    {
        err = "Policy index is not valid";
        return -1;
    }

    remove_saved_policy(policies[index]->filename);
    if (policies[index])
        delete policies[index];
    policies.erase(policies.begin() + index);
    return 0;
}

void Policies::clear_policies()
{
    for (size_t index = 0; index < policies.size(); ++index)
    {
        if (policies[index])
        {
            remove_saved_policy(policies[index]->filename);
            delete policies[index];
        }
    }
    policies.clear();
}

int Policies::policy_change_name(int id, const std::string& name, const std::string& description, std::string& err)
{
    if (id < 0 || id > (int)policies.size())
    {
        err = "policy id is not existing";
        return -1;
    }

    Policy *p = policies[id];
    if (!p)
    {
        err = "policy id is not existing anymore";
        return -1;
    }

    p->name = name;
    p->description = description;

    return 0;
}

xmlDocPtr Policies::create_doc(size_t pos)
{
    if (pos >= policies.size() || !policies[pos])
        return NULL;

    return policies[pos]->create_doc();
}

bool Policies::policy_exists(const std::string& policy)
{
    for (size_t i =0; i < policies.size(); ++i)
        if (policies[i]->name == policy)
            return true;

    return false;
}

int Policies::create_xslt_policy_from_file(const std::string& file, std::string& err)
{
    std::bitset<MediaConchLib::report_Max> report_set;
    std::vector<std::string> files;
    std::map<std::string, std::string> options;
    std::vector<std::string> policies_vec;
    MediaConchLib::ReportRes result;

    report_set.set(MediaConchLib::report_MediaInfo);
    files.push_back(file);

    core->get_report(report_set, MediaConchLib::format_Xml, files,
                     policies_vec, policies_vec,
                     options, &result,
                     NULL, NULL);
    if (!result.valid || !result.report.length())
    {
        err = "Implementation report not found";
        return -1;
    }

    Policy *p = new XsltPolicy(this, !core->accepts_https());

    //Policy filename
    find_save_name(NULL, p->filename);

    size_t name_pos = file.rfind("/");
    if (name_pos == std::string::npos)
        name_pos = 0;
    else
        name_pos++;
    p->name = file.substr(name_pos, std::string::npos);

    int pos = -1;
    if (((XsltPolicy*)p)->create_policy_from_mi(result.report) < 0)
        delete p;
    else
    {
        pos = (int)policies.size();
        policies.push_back(p);
    }

    return pos;
}

// XSLT Rule
int Policies::create_xslt_policy_rule(int policy_id, std::string& err)
{
    if (policy_id < 0 || policy_id > (int)policies.size())
    {
        err = "policy id is not existing";
        return -1;
    }
    XsltPolicy *p = (XsltPolicy *)policies[policy_id];

    if (!p)
    {
        err = "policy id is not existing anymore";
        return -1;
    }

    if (p->type != POLICY_XSLT)
    {
        err = "policy rule cannot be added";
        return -1;
    }
    // XsltRule *rule = new XsltRule();

    // rule->title = "New Rule";
    // size_t pos = p->rules.size();
    // p->rules.push_back(rule);

    // return (int)pos;
    return -1;
}

int Policies::edit_xslt_policy_rule(int policy_id, int rule_id, const XsltPolicyRule *rule, std::string& err)
{
    if (policy_id < 0 || policy_id > (int)policies.size())
    {
        err = "policy id is not existing";
        return -1;
    }
    XsltPolicy *p = (XsltPolicy*)policies[policy_id];

    if (!p)
    {
        err = "policy id is not existing anymore";
        return -1;
    }

    if (p->type != POLICY_XSLT)
    {
        err = "policy rule cannot be duplicate";
        return -1;
    }

    // if (rule_id < 0 || rule_id > (int)p->rules.size())
    // {
    //     err = "rule id is not existing";
    //     return -1;
    // }

    // if (p->rules[rule_id])
    //     *p->rules[rule_id] = *rule;
    // else
    //     p->rules[rule_id] = new XsltRule(*rule);

    return 0;
}

int Policies::duplicate_xslt_policy_rule(int policy_id, int rule_id, std::string& err)
{
    if (policy_id < 0 || policy_id > (int)policies.size())
    {
        err = "policy id is not existing";
        return -1;
    }
    XsltPolicy *p = (XsltPolicy*)policies[policy_id];

    if (!p)
    {
        err = "policy id is not existing anymore";
        return -1;
    }

    if (p->type != POLICY_XSLT)
    {
        err = "policy rule cannot be duplicate";
        return -1;
    }

    // if (rule_id < 0 || rule_id > (int)p->rules.size())
    // {
    //     err = "rule id is not existing";
    //     return -1;
    // }
    // XsltRule *r = p->rules[rule_id];

    // if (!r)
    // {
    //     err = "rule is not existing anymore";
    //     return -1;
    // }
    // XsltRule *rule = new XsltRule(*r);

    // rule->title += " - duplicate";
    // size_t pos = p->rules.size();
    // p->rules.push_back(rule);

    // return (int)pos;
    return -1;
}

int Policies::delete_xslt_policy_rule(int policy_id, int rule_id, std::string& err)
{
    if (policy_id < 0 || policy_id > (int)policies.size())
    {
        err = "policy id is not existing";
        return -1;
    }
    XsltPolicy *p = (XsltPolicy*)policies[policy_id];

    if (!p)
    {
        err = "policy id is not existing anymore";
        return -1;
    }

    if (p->type != POLICY_XSLT)
    {
        err = "policy rule cannot be duplicate";
        return -1;
    }

    // if (rule_id < 0 || rule_id > (int)p->rules.size())
    // {
    //     err = "rule id is not existing";
    //     return -1;
    // }

    // if (p->rules[rule_id])
    //     delete p->rules[rule_id];
    // p->rules.erase(p->rules.begin() + rule_id);

    return 0;
}

// Helper
void Policies::create_values_from_csv()
{
    ZenLib::ZtringList list;
    list.Separator_Set(0, __T(","));
    list.Write(core->Menu_Option_Preferences_Option(__T("MAXML_StreamKinds"), ZenLib::Ztring()));
    for (size_t i = 0; i < list.size(); ++i)
    {
        std::list<std::string> fields;
        fields.push_back("");

        ZenLib::ZtringList listField;
        listField.Separator_Set(0, __T(","));
        listField.Write(core->Menu_Option_Preferences_Option(__T("MAXML_Fields"), list[i]));
        for (size_t j = 0; j < listField.size(); ++j)
            fields.push_back(listField[j].To_UTF8());

        existing_type[list[i].To_UTF8()] = fields;
    }

    validatorType validators[] = {
        { "", "", "" },
        { "=", "is_equal", "Equal" },
        { "!=", "is_not_equal", "Not Equal" },
        { ">", "is_greater_than", "Greater than" },
        { ">=", "is_greater_or_equal_than", "Greater or equal" },
        { "<", "is_less_than", "Less" },
        { "<=", "is_less_or_equal_than", "Less or equal" }
    };

    for (size_t i=0; i < (sizeof(validators) / sizeof(*validators)); i++)
        existing_validator.push_back(validators[i]);

    std::string xsltOperators[] =
    {
        "is_equal",
        "is_not_equal",
        "is_greater_than",
        "is_less_than",
        "is_greater_or_equal_than",
        "is_less_or_equal_than",
        "exists",
        "does_not_exist",
        "contains_string",
        "is_true",
    };

    for (size_t i=0; i < (sizeof(xsltOperators) / sizeof(*xsltOperators)); i++)
        existing_xsltOperator.push_back(xsltOperators[i]);
}

bool Policies::check_test_type(const std::string& type)
{
    std::map<std::string, std::list<std::string> >::iterator it = existing_type.begin();
    std::map<std::string, std::list<std::string> >::iterator ite = existing_type.end();

    for (; it != ite; ++it)
        if (!type.compare(it->first))
            return true;

    return false;
}

bool Policies::check_test_field(const std::string& field)
{
    std::map<std::string, std::list<std::string> >::iterator itType = existing_type.begin();
    std::map<std::string, std::list<std::string> >::iterator iteType = existing_type.end();

    for (; itType != iteType; ++itType)
    {
        std::list<std::string>::iterator it = itType->second.begin();
        std::list<std::string>::iterator ite = itType->second.end();

        for (; it != ite; ++it)
            if (!field.compare(*it))
                return true;
    }

    return false;
}

bool Policies::check_test_validator(const std::string& validator)
{
    std::list<validatorType>::iterator it = existing_validator.begin();
    std::list<validatorType>::iterator ite = existing_validator.end();

    for (; it != ite; ++it)
        if (!validator.compare(it->value))
            return true;

    return false;
}

std::string Policies::parse_test_value(std::string& sub, const std::string& before, const std::string& after)
{
    if (sub.compare(0, before.length(), before))
        return std::string();

    sub = sub.substr(before.length());
    std::string::size_type pos = sub.find(after);
    if (pos == std::string::npos)
        return std::string();

    std::string ret = sub.substr(0, pos);
    sub = sub.substr(pos + after.length());

    return ret;
}

std::string Policies::parse_test_field(std::string& sub, const std::string& before)
{
    if (sub.compare(0, before.length(), before))
        return std::string();

    sub = sub.substr(before.length());
    if (!sub.length())
        return std::string();

    std::string::size_type pos = sub.find(" ");
    std::string ret = sub.substr(0, pos);
    if (pos == std::string::npos)
    {
        sub = sub.substr(sub.length());
        return ret;
    }
    sub = sub.substr(pos + 1);

    return ret;
}

void Policies::find_save_name(const char* basename, std::string& save_name)
{
    std::string data_path = Core::get_local_data_path();

    data_path += "policies/";
    if (basename)
    {
        std::string base(basename);
        ZenLib::Ztring z_path = ZenLib::Ztring().From_UTF8(base);
        if (base.find(data_path) == 0 && ZenLib::File::Exists(z_path))
        {
            save_name = basename;
            return;
        }
    }

    for (size_t i = 0; 1; ++i)
    {
        std::stringstream ss;
        ss << data_path << "policy";
        if (i)
            ss << i;
        ss << ".xsl";

        ZenLib::Ztring z_path = ZenLib::Ztring().From_UTF8(ss.str());
        if (!ZenLib::File::Exists(z_path))
        {
            save_name = ss.str();
            break;
        }
    }
}

void Policies::find_new_policy_name(std::string& name)
{
    name = "New policy";
    for (size_t i = 0; 1; ++i)
    {
        std::stringstream ss;
        ss << name;
        if (i)
            ss << " " << i;

        size_t j = 0;
        for (; j < policies.size(); ++j)
            if (policies[j] && policies[j]->name == ss.str())
                break;
        if (j == policies.size())
        {
            name = ss.str();
            break;
        }
    }
}

void Policies::remove_saved_policy(const std::string& saved_name)
{
    ZenLib::Ztring z_path = ZenLib::Ztring().From_UTF8(saved_name);
    if (!ZenLib::File::Exists(z_path))
        return;

    ZenLib::File::Delete(z_path);
}

}
