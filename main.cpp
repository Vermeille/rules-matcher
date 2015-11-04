#include <vector>
#include <string>
#include <utility>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <fstream>

#include <nlp-common/rules-matcher.h>

#include <httpi/job.h>
#include <httpi/displayer.h>

static const unsigned int kNotFound = -1;

RulesMatcher g_rm;

static const JobDesc match = {
    { { "input", "text", "Text to match" } },
    "Match",  // name
    "/match",  // url
    "Match the input against the rules",  // longer description
    true /* synchronous */,
    true /* reentrant */,
    [](const std::vector<std::string>& vs, JobResult& job) { // the actual function
        std::istringstream input(vs[0]);
        std::string w;
        TrainingExample ex;
        input >> w;
        while (input) {
            ex.inputs.push_back({0, w});
            input >> w;
        }

        Html html;

        html << Ul();
        for (auto& r : g_rm.Match(ex)) {
            html << Li() << r << Close();
        }
        html << Close();

        job.SetPage(html);
    },
};

static const JobDesc load = {
    { { "rules", "file", "A file containing rules" } },
    "Load",
    "/load",
    "Load rules",
    true /* synchronous */,
    false /* reentrant */,
    [](const std::vector<std::string>& vs, JobResult& job) {
        g_rm = RulesMatcher::FromSerialized(vs[0]);
        job.SetPage(Html() << "done");
    },
};

static const JobDesc add = {
    { { "label", "text", "The label" },
      { "pattern", "text", "The pattern" } },
    "Add",
    "/Add",
    "Add rules",
    true /* synchronous */,
    false /* reentrant */,
    [](const std::vector<std::string>& vs, JobResult& job) {
        g_rm.AddRule(vs[0] + " : " + vs[1]);
        job.SetPage(Html() << "done");
    },
};

Html Save(const std::string&, const POSTValues&) {
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

Html DisplayRules(const std::string&, const POSTValues&) {
    Html html;
    html << H2() << "Rules" << Close();

    html <<
    Tag("table").AddClass("table") <<
        Tag("tr") <<
            Tag("th") << "Label" << Close() <<
            Tag("th") << "Pattern" << Close() <<
        Close();

    for (auto& r : g_rm) {
        html <<
        Tag("tr") <<
            Tag("td") << r.key() << Close() <<
            Tag("td") << r.AsString() << Close() <<
        Close();
    }
    html << Close();
    return html;
}

int main(int argc, char** argv) {
    InitHttpInterface();  // Init the http server
    RegisterJob(match);
    RegisterJob(load);
    RegisterUrl("/rules", DisplayRules);
    RegisterUrl("/save", Save);
    ServiceLoopForever();  // infinite loop ending only on SIGINT / SIGTERM / SIGKILL
    StopHttpInterface();  // clear resources
    return 0;
}
