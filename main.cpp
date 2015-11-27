#include <vector>
#include <string>
#include <utility>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <fstream>

#include <nlp-common/rules-matcher.h>
#include <nlp-common/tokenizer.h>

#include <httpi/rest-helpers.h>
#include <httpi/html/html.h>
#include <httpi/html/json.h>
#include <httpi/webjob.h>
#include <httpi/displayer.h>
#include <httpi/html/form-gen.h>
#include <httpi/monitoring.h>

static const unsigned int kNotFound = -1;


namespace htmli = httpi::html;

RulesMatcher CreateMatcher(const std::string& matcher) {
    return RulesMatcher::FromSerialized(matcher);
}

htmli::Html Save(RulesMatcher& rm) {
    using namespace htmli;
    return Html() << A().Id("dl").Attr("download", "rules_model.bin") << "Download Model" << Close() <<
        Tag("textarea").Id("content").Attr("style", "display: none") << rm.Serialize() << Close() <<
        Tag("script") <<
            "window.onload = function() {"
                "var txt = document.getElementById('dl');"
                "txt.href = 'data:text/plain;charset=utf-8,' "
                    "+ encodeURIComponent(document.getElementById('content').value);"
                "};" <<
        Close();
}

htmli::Html DisplayRules(RulesMatcher& rm) {
    using namespace htmli;
    Html html;
    html << H2() << "Rules" << Close();

    html <<
    Table().AddClass("table") <<
        Tr() <<
            Th() << "Label" << Close() <<
            Th() << "Pattern" << Close() <<
        Close();

    for (auto& r : rm) {
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
    using namespace httpi::html;
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
                    "<h2>Resources</h2>"
                    "<div class=\"col-md-3\">" <<
                        Ul() <<
                            Li() <<
                                A().Attr("href", "/model") << "Model" << Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href", "/rules") << "Rules" << Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href", "/match") << "try it" << Close() <<
                            Close() <<
                        Close() <<
                    "</div>"
                "</div>"
            "</body>"
        "</html>").Get();
}

int main(int argc, char** argv) {
    InitHttpInterface();  // Init the http server

    RulesMatcher rules_matcher;
    WebJobsPool jp;
    auto t1 = jp.StartJob(std::unique_ptr<MonitoringJob>(new MonitoringJob()));
    auto monitoring_job = jp.GetId(t1);

    RegisterUrl("/", [&monitoring_job](const std::string&, const POSTValues&) {
            return MakePage(*monitoring_job->job_data().page());
        });

    RegisterUrl("/match", httpi::RestPageMaker(MakePage)
        .AddResource("GET", httpi::RestResource(
            htmli::FormDescriptor<std::string> {
                "GET", "/match",
                "Match",
                "Match the input against the rules",
                { { "input", "text", "Text to match" } }
            },
            [&rules_matcher](const std::string& input) {
                TrainingExample ex;
                ex.inputs = Tokenizer::FR(input);
                return rules_matcher.Match(ex);
            },
            [](const std::vector<std::string>& matches) {
                using namespace httpi::html;
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
            },
            [](const std::vector<std::string>& matches) {
                return JsonBuilder().Append("matches", matches).Build();
            })));

    RegisterUrl("/model", httpi::RestPageMaker(MakePage)
        .AddResource("GET", httpi::RestResource(
            htmli::FormDescriptor<> {},
            []() {
                return 0;
            },
            [&rules_matcher](int) {
                return Save(rules_matcher);
            },
            [](int) {
                return JsonBuilder().Append("result", 1).Build();
            }))
        .AddResource("PUT", httpi::RestResource(
            htmli::FormDescriptor<std::string> {
                "PUT", "/model",
                "Load",
                "Load rules",
                { { "rules", "file", "A file containing rules" } }
            },
            [&rules_matcher](const std::string& model) {
                rules_matcher = CreateMatcher(model);
                return 0;
            },
            [](int) {
                return htmli::Html() << "model loaded";
            },
            [](int) {
                return JsonBuilder().Append("result", 0).Build();
            })));

    RegisterUrl("/rules", httpi::RestPageMaker(MakePage)
        .AddResource("POST", httpi::RestResource(
            htmli::FormDescriptor<std::string, std::string> {
                "POST", "/rules",
                "Add",
                "Add rules",
                { { "label", "text", "The label" },
                  { "pattern", "text", "The pattern" } }
            },
            [&rules_matcher](const std::string& label, const std::string& pattern) {
                rules_matcher.AddRule(label + " : " + pattern);
                return 0;
            },
            [](int) {
                return htmli::Html() << "Rule added";
            },
            [](int) {
                return JsonBuilder().Append("result", 0).Build();
            }))
        .AddResource("GET", httpi::RestResource(
            htmli::FormDescriptor<>{},
            [](){ return 0; },
            [&rules_matcher](int) {
                return DisplayRules(rules_matcher);
            },
            [](int) {
                return JsonBuilder().Append("result", 1).Build();
            })));

    ServiceLoopForever();  // infinite loop ending only on SIGINT / SIGTERM / SIGKILL
    StopHttpInterface();  // clear resources
    return 0;
}
