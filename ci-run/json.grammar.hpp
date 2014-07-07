#pragma once

#include <boost/spirit/include/qi.hpp>

namespace Json
{
    namespace spirit = boost::spirit;

    template <typename Iterator>
    struct JsonGrammar : spirit::qi::grammar<Iterator, Value(), spirit::ascii::space_type>
    {
        JsonGrammar() : JsonGrammar::base_type(jsonValue)
        {
            using namespace spirit::qi::labels;

            jsonFloat %= spirit::qi::double_;
            jsonInteger %= spirit::qi::int_;

            jsonValue %=
                (
                    /*
                      jsonString
                    | jsonNumber
                    | jsonObject
                    | jsonArray
                    | jsonBoolean
                    | jsonNull
                    | jsonUndefined
                    */
                      jsonFloat
                    | jsonInteger
                );
        }

        spirit::qi::rule<Iterator, Integer(), spirit::ascii::space_type> jsonInteger;
        spirit::qi::rule<Iterator, Float(), spirit::ascii::space_type> jsonFloat;
        spirit::qi::rule<Iterator, String(), spirit::ascii::space_type> jsonString;
        spirit::qi::rule<Iterator, Boolean(), spirit::ascii::space_type> jsonBoolean;
        spirit::qi::rule<Iterator, Null(), spirit::ascii::space_type> jsonNull;

        spirit::qi::rule<Iterator, Object(), spirit::ascii::space_type> jsonObject;
        spirit::qi::rule<Iterator, Array(), spirit::ascii::space_type> jsonArray;

        spirit::qi::rule<Iterator, Value(), spirit::ascii::space_type> jsonValue;
    };
}
