// The test report: one HTML document per analysis, the source of both the
// saved .html and the PDF (the window prints this same document).
//
// What a report must carry is what results-v1 says a result is: the value
// as reported, or why there is none; the conformity and every reason; and
// enough provenance (file hash, profile version, build) to find the data
// again. Built from the synthetic fixture with known answers.

#include "core/Report.h"
#include "core/Session.h"
#include "check.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace ts::core;

#ifndef TS_FIXTURES
#error "TS_FIXTURES must point at tests/fixtures"
#endif

namespace {

Session analysed()
{
    Session s;
    s.openData(QString(TS_FIXTURES) + "/synthetic/ro-continuous.csv");
    s.specimen.id = "RO-1";
    s.specimen.material = "synthetic steel";
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 78.54e-6;
    s.specimen.gaugeLength = 50e-3;
    s.profile = "iso-6892-1-2019";
    s.analyse();
    return s;
}

} // namespace

TS_CASE(the_report_states_conformity_and_every_reason)
{
    const Session s = analysed();
    TS_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TS_CHECK(html.contains("Aligned with ISO 6892-1:2019"));
    TS_CHECK(html.contains("not to the standard"));
    const QJsonArray reasons =
        s.analysis()->doc.value("conformity").toObject().value("reasons").toArray();
    TS_REQUIRE(!reasons.isEmpty());
    for (const QJsonValue &r : reasons)
        if (!html.contains(r.toObject().value("code").toString()))
            ts::test::fail(__FILE__, __LINE__,
                           "reason missing: " + r.toObject().value("code").toString().toStdString());
}

TS_CASE(the_report_gives_each_result_as_reported_or_why_not)
{
    const Session s = analysed();
    TS_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TS_CHECK(html.contains("<td>300</td>"));  // Rp0.2, the fixture's answer
    TS_CHECK(html.contains("<td>500</td>"));  // Rm
    for (const QJsonValue &v : s.analysis()->doc.value("results").toArray()) {
        const QJsonObject r = v.toObject();
        if (r.value("status").toString() != "determined"
            && !html.contains(r.value("reason").toString().toHtmlEscaped()))
            ts::test::fail(__FILE__, __LINE__,
                           "no reason for " + r.value("id").toString().toStdString());
    }
}

TS_CASE(the_report_carries_specimen_source_profile_and_build)
{
    const Session s = analysed();
    TS_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TS_CHECK(html.contains("RO-1"));
    TS_CHECK(html.contains("synthetic steel"));
    TS_CHECK(html.contains("78.54 mm2"));
    TS_CHECK(html.contains("50 mm"));
    TS_CHECK(html.contains("ro-continuous.csv"));
    TS_CHECK(html.contains(QString::fromLatin1(s.table()->sha256)));
    TS_CHECK(html.contains("iso-6892-1-2019"));
    TS_CHECK(html.contains(s.analysis()->profile.profileVersion));
    TS_CHECK(html.contains(QString::fromLatin1(s.analysis()->profile.sha256)));
    TS_CHECK(html.contains("TenSense"));
}

TS_CASE(text_from_files_and_users_is_escaped)
{
    Session s = analysed();
    TS_REQUIRE(s.analysis().has_value());
    Specimen sp = s.specimen;
    sp.id = "<script>x</script>";
    const QString html = reportHtml(s.analysis()->doc, sp, {});
    TS_CHECK(!html.contains("<script>"));
    TS_CHECK(html.contains("&lt;script&gt;"));
}

TS_CASE(the_chart_appears_only_when_given)
{
    const Session s = analysed();
    TS_REQUIRE(s.analysis().has_value());
    TS_CHECK(!reportHtml(s.analysis()->doc, s.specimen, {}).contains("<img"));
    TS_CHECK(reportHtml(s.analysis()->doc, s.specimen, "chart.png").contains("src=\"chart.png\""));
}

TS_CASE(the_report_has_no_em_or_en_dashes)
{
    const Session s = analysed();
    TS_REQUIRE(s.analysis().has_value());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    TS_CHECK(!html.contains(QChar(0x2014)));
    TS_CHECK(!html.contains(QChar(0x2013)));
}

TS_CASE(the_files_own_results_are_shown_apart_and_as_written)
{
    // Bluehill Universal's results table (tests/fixtures/instron): shown in
    // its own section, value and unit as Bluehill wrote them, never among
    // TenSense's results; a file without one gets no such section.
    Session s;
    s.profile = "iso-527-1-2019";
    TS_REQUIRE(s.openData(QString(TS_FIXTURES) + "/instron/bluehill-universal-results.csv").isEmpty());
    s.specimen.shape = Specimen::Shape::GivenArea;
    s.specimen.area = 122.5e-6;
    TS_REQUIRE(s.analyse());
    const QString html = reportHtml(s.analysis()->doc, s.specimen, {});
    const int own = html.indexOf("<h2>Results in the file");
    TS_REQUIRE(own > html.indexOf("<h2>Results</h2>"));
    const QString section = html.mid(own, html.indexOf("</table>", own) - own);
    TS_CHECK(section.contains("Maximum Force"));
    TS_CHECK(section.contains("285.30"));
    TS_CHECK(section.contains("Modulus (Automatic Young&#39;s)") || section.contains("Modulus (Automatic Young's)"));
    TS_CHECK(section.contains("-----"));
    TS_CHECK(section.contains("not checked"));
    TS_CHECK(!analysed().analysis()->doc.contains("source_results"));
    TS_CHECK(!reportHtml(analysed().analysis()->doc, analysed().specimen, {}).contains("Results in the file"));
}

int main() { return ts::test::run(); }
