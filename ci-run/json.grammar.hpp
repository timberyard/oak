template <typename Iterator>
struct JsonGrammar : qi::grammar<Iterator, Value(), ascii::space_type>
{
    JsonGrammar() : JsonGrammar::base_type(xml)
    {
        using qi::lit;
        using qi::lexeme;
        using ascii::char_;
        using ascii::string;
        using namespace qi::labels;

        _value %=
            (
                  _string
                | _number
                | _object
                | _array
                | _boolean
                | _null
                | _undefined
            );
    }

    qi::rule<Iterator, Value(), ascii::space_type> _value;

    qi::rule<Iterator, String(), ascii::space_type> _string;
    qi::rule<Iterator, Real(), ascii::space_type> _real;
    qi::rule<Iterator, Integer(), ascii::space_type> _integer;
    qi::rule<Iterator, Object(), ascii::space_type> _object;
    qi::rule<Iterator, Array(), ascii::space_type> _array;
    qi::rule<Iterator, Boolean(), ascii::space_type> _boolean;
    qi::rule<Iterator, Null(), ascii::space_type> _null;
    qi::rule<Iterator, Undefined(), ascii::space_type> _undefined;
};