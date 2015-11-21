#include <vector>
#include <string>
#include <utility>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <fstream>

#include <nlp-common/rules-matcher.h>

#include <httpi/html/html.h>
#include <httpi/webjob.h>
#include <httpi/displayer.h>
#include <httpi/html/form-gen.h>
#include <httpi/monitoring.h>

static const unsigned int kNotFound = -1;

RulesMatcher g_rm;

namespace htmli = httpi::html;

static const htmli::FormDescriptor match_form_desc = {
    "Match",  // name
    "Match the input against the rules",  // longer description
    { { "input", "text", "Text to match" } }
};

static const std::string match_form = match_form_desc
        .MakeForm("/match", "GET").Get();

htmli::Html SeeMatches(const std::string& text) {
    using namespace htmli;

    std::istringstream input(text);
    std::string w;
    TrainingExample ex;
    // FIXME: use tokenizer
    input >> w;
    while (input) {
        ex.inputs.push_back({w});
        input >> w;
    }
    auto matches = g_rm.Match(ex);

    Html html;
    if (matches.size() == 0) {
        html << P() << "no match" << Close();
    } else {
        html << Ul();
        for (auto& r : matches) {
            html << Li() << r << Close();
        }
        html << Close();
    }

    return html;
}

static const htmli::FormDescriptor load_form_desc = {
    "Load",
    "Load rules",
    { { "rules", "file", "A file containing rules" } }
};

static const std::string load_form = load_form_desc
        .MakeForm("/model", "PUT").Get();

htmli::Html CreateMatcher(const std::string& matcher) {
    g_rm = RulesMatcher::FromSerialized(matcher);
}

static const htmli::FormDescriptor add_form_desc = {
    "Add",
    "Add rules",
    { { "label", "text", "The label" },
      { "pattern", "text", "The pattern" } }
};

static const std::string add_form = add_form_desc
        .MakeForm("/rules", "POST").Get();

htmli::Html Save() {
    using namespace htmli;
    return Html() << A().Id("dl").Attr("download", "bow_model.bin") << "Download Model" << Close() <<
        Tag("textarea").Id("content").Attr("style", "display: none") << g_rm.Serialize() << Close() <<
        Tag("script") <<
            "window.onload = function() {"
                "var txt = document.getElementById('dl');"
                "txt.href = 'data:text/plain;charset=utf-8,' "
                    "+ encodeURIComponent(document.getElementById('content').value);"
                "};" <<
        Close();
}

htmli::Html DisplayRules() {
    using namespace htmli;
    Html html;
    html << H2() << "Rules" << Close();

    html <<
    Table().AddClass("table") <<
        Tr() <<
            Th() << "Label" << Close() <<
            Th() << "Pattern" << Close() <<
        Close();

    for (auto& r : g_rm) {
        html <<
        Tr() <<
            Td() << r.key() << Close() <<
            Td() << r.AsString() << Close() <<
        Close();
    }
    html << Close();
    return html;
}

std::string MakePage(const std::string& content) {
    return (httpi::html::Html() <<
        "<!DOCTYPE html>"
        "<html>"
           "<head>"
                R"(<meta charset="utf-8">)"
                R"(<meta http-equiv="X-UA-Compatible" content="IE=edge">)"
                R"(<meta name="viewport" content="width=device-width, initial-scale=1">)"
                R"(<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css">)"
                R"(<link rel="stylesheet" href="//cdn.jsdelivr.net/chartist.js/latest/chartist.min.css">)"
                R"(<script src="//cdn.jsdelivr.net/chartist.js/latest/chartist.min.js"></script>)"
            "</head>"
            "<body lang=\"en\">"
                "<div class=\"container\">"
                    "<div class=\"col-md-9\">" <<
                        content <<
                    "</div>"
                    "<div class=\"col-md-3\">" <<
                        "JOBS"
                    "</div>"
                "</div>"
            "</body>"
        "</html>").Get();
}

int main(int argc, char** argv) {
    InitHttpInterface();  // Init the http server

    WebJobsPool jp;
    auto t1 = jp.StartJob(std::unique_ptr<MonitoringJob>(new MonitoringJob()));
    auto monitoring_job = jp.GetId(t1);

    RegisterUrl("/", [&monitoring_job](const std::string&, const POSTValues&) {
            return MakePage(*monitoring_job->job_data().page());
        });

    RegisterUrl("/match",
            [](const std::string& method, const POSTValues& args) {
                using namespace httpi::html;
                Html html;

                auto vargs = match_form_desc.ValidateParams(args);
                if (std::get<0>(vargs)) {
                    html << P().AddClass("alert") << "No input" << Close();
                } else {
                    html << SeeMatches(std::get<2>(vargs)[0]);
                }
                html << match_form;
                return MakePage(html.Get());
            });

    RegisterUrl("/model", [](const std::string& method, const POSTValues& args) {
            using namespace httpi::html;
            Html html;
            if (method == "GET") {
                html << Save();
            } else if (method == "PUT") {
                auto vargs = load_form_desc.ValidateParams(args);
                if (std::get<0>(vargs)) {
                    html << (std::get<1>(vargs).Get());
                } else {
                    CreateMatcher(std::get<2>(vargs)[0]);
                    html << "Model loaded";
                }
            } else {
                html << "no such method";
            }
            html << load_form;
            return MakePage(html.Get());
        });

    RegisterUrl("/rules",
            [](const std::string& method, const POSTValues& args) {
                using namespace httpi::html;
                Html html;

                if (method == "POST") {
                    auto vargs = add_form_desc.ValidateParams(args);
                    if (std::get<0>(vargs)) {
                        html << std::get<1>(vargs).Get();
                    } else {
                        auto& vs = std::get<2>(vargs);
                        g_rm.AddRule(vs[0] + " : " + vs[1]);
                    }
                } else {
                    html << "no such method";
                }
                html << DisplayRules().Get() << add_form;
                return MakePage(html.Get());
            });

    ServiceLoopForever();  // infinite loop ending only on SIGINT / SIGTERM / SIGKILL
    StopHttpInterface();  // clear resources
    return 0;
}
