#!/bin/sh

UTILS_PATH=$(dirname "$0")
MC_ROOT_PATH=$UTILS_PATH/../../../../
RESOURCE_PATH=$MC_ROOT_PATH/Source/Resource

cmd_is_ok()
{
    if test $? -ne 0
    then
        exit 1;
    fi
}

cmd_is_ko()
{
    if test $? -eq 0
    then
        exit 1;
    fi
}

xml_is_correct()
{
    $(echo "$1" | xmllint --noout - 2> /dev/null)
    if test $? -ne 0
    then
        exit 1;
    fi
}

xml_is_invalid()
{
    $(echo "$1" | xmllint --noout - 2> /dev/null)
    if test $? -eq 0
    then
        exit 1;
    fi
}

output_has_mc_xml()
{
    if [ $(echo "$1" | grep -oc "<MediaConch") -eq 0 ]
    then
        exit 1;
    fi
}

output_has_not_mc_xml()
{
    if [ $(echo "$1" | grep -oc "<MediaConch") -ne 0 ]
    then
        exit 1;
    fi
}

output_has_mt_xml()
{
    if [ $(echo "$1" | grep -oc "<MediaTrace") -eq 0 ]
    then
        exit 1;
    fi
}

output_has_not_mt_xml()
{
    if [ $(echo "$1" | grep -oc "<MediaTrace") -ne 0 ]
    then
        exit 1;
    fi
}

output_has_mi_xml()
{
    if [ $(echo "$1" | grep -oc "<MediaInfo") -eq 0 ]
    then
        exit 1;
    fi
}

output_has_not_mi_xml()
{
    if [ $(echo "$1" | grep -oc "<MediaInfo") -ne 0 ]
    then
        exit 1;
    fi
}
