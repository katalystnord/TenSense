// Reading machine exports: the dialects TenSense meets first.
//
// The fixtures are hand-written, small, and in the layouts of the files the
// validation uses: a quoted preamble with semicolons (NPL TENSTAND's
// layout), a decimal comma with a units row in brackets and an empty cell
// where an extensometer came off (BAM's layout), and a plain CSV with the
// unit in the column name (TenSense's synthetic fixtures).

#include "core/Import.h"
#include "check.h"

#include <QCryptographicHash>
#include <QFile>

#include <cmath>

using namespace ts::core;

#ifndef TS_FIXTURES
#error "TS_FIXTURES must point at tests/fixtures"
#endif

static QString fixture(const char *rel) { return QString(TS_FIXTURES) + "/" + rel; }

TS_CASE(quoted_preamble_layout_reads_names_units_preamble_and_rows)
{
    const auto r = readTable(fixture("import/quoted-preamble.txt"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.delimiter == ';');
    TS_CHECK(t.decimal == '.');
    TS_CHECK(t.names == QStringList({"time", "crosshead", "extensometer", "force"}));
    TS_CHECK(t.units == QStringList({"s", "mm", "mm", "kN"}));
    TS_REQUIRE(t.columns.size() == 4);
    TS_CHECK(t.columns[3].size() == 4);
    TS_NEAR(t.columns[3][2], 2.9, 1e-12);
    bool so = false;
    for (const auto &p : t.preamble)
        if (p.key == "So" && p.value == "78.5" && p.unit == "mm2")
            so = true;
    TS_CHECK(so);
}

TS_CASE(decimal_comma_layout_reads_bracketed_units_and_keeps_empty_cells_empty)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.decimal == ',');
    TS_CHECK(t.units == QStringList({"sec", "mm", "kN", "%"}));
    TS_NEAR(t.columns[2][2], 2.75, 1e-12);
    TS_CHECK(std::isnan(t.columns[3][3]));  // empty, not zero
}

TS_CASE(the_file_hash_is_of_the_bytes_read)
{
    const QString path = fixture("import/decimal-comma.csv");
    const auto r = readTable(path);
    TS_REQUIRE(r.table.has_value());
    QFile f(path);
    TS_REQUIRE(f.open(QIODevice::ReadOnly));
    TS_CHECK(r.table->sha256
             == QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex());
}

TS_CASE(unit_in_the_column_name_is_found)
{
    const auto r = readTable(fixture("synthetic/ro-continuous.csv"));
    TS_REQUIRE(r.table.has_value());
    const auto imp = toTension(*r.table, TensionMapping{{}, {}, {}, {},
                                                        78.54e-6, 50e-3});
    TS_REQUIRE(imp.error.isEmpty());
    TS_CHECK(imp.forceColumn == "force_kN");
    TS_NEAR(imp.input.force[2], 2 * 0.5 * 78.54, 1e-6);  // 1 MPa * 78.54 mm2 = 78.54 N
}

TS_CASE(tension_mapping_takes_geometry_from_the_preamble_in_si)
{
    const auto r = readTable(fixture("import/quoted-preamble.txt"));
    TS_REQUIRE(r.table.has_value());
    const auto imp = toTension(*r.table, TensionMapping{});
    TS_REQUIRE(imp.error.isEmpty());
    TS_CHECK(imp.forceColumn == "force");
    TS_CHECK(imp.extensionColumn == "extensometer");  // not the crosshead
    TS_NEAR(imp.input.area, 78.5e-6, 1e-15);
    TS_NEAR(imp.input.gaugeLength, 0.05, 1e-15);
    TS_NEAR(imp.input.force[1], 1500.0, 1e-9);
    TS_NEAR(imp.input.extension[3], 15e-6, 1e-15);
}

TS_CASE(percent_strain_column_is_taken_as_strain_with_a_note)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TS_REQUIRE(r.table.has_value());
    TensionMapping m;
    m.area = 120.6e-6;
    const auto imp = toTension(*r.table, m);
    TS_REQUIRE(imp.error.isEmpty());
    TS_CHECK(imp.extensionColumn == "Extensometer elongation");
    // 0.07 % strain: with the gauge length set to 1, extension is the strain.
    TS_NEAR(imp.input.extension[2] / imp.input.gaugeLength, 0.0007, 1e-15);
    TS_CHECK(std::isnan(imp.input.extension[3]));
    TS_CHECK(!imp.notes.filter("strain").isEmpty());
}

TS_CASE(missing_area_is_an_error_that_names_it)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TS_REQUIRE(r.table.has_value());
    const auto imp = toTension(*r.table, TensionMapping{});
    TS_CHECK(imp.error.contains("area"));
}

TS_CASE(unknown_column_is_an_error_that_lists_the_columns)
{
    const auto r = readTable(fixture("import/decimal-comma.csv"));
    TS_REQUIRE(r.table.has_value());
    TensionMapping m;
    m.force = "Kraft";
    m.area = 1e-4;
    const auto imp = toTension(*r.table, m);
    TS_CHECK(imp.error.contains("Kraft"));
    TS_CHECK(imp.error.contains("Load"));
}

TS_CASE(quantities_parse_with_their_units)
{
    TS_NEAR(parseQuantity("120.6 mm2", "area").value_or(NAN), 120.6e-6, 1e-15);
    TS_NEAR(parseQuantity("50mm", "length").value_or(NAN), 0.05, 1e-15);
    TS_NEAR(parseQuantity("1,5 kN", "force").value_or(NAN), 1500, 1e-9);
    TS_CHECK(!parseQuantity("50", "length"));       // a unit is required
    TS_CHECK(!parseQuantity("50 kN", "length"));    // and must fit the dimension
}

// --- Real exports from compression and bending machines (Phase 3) ------------
// A ZwickRoell testXpert III export (TU Braunschweig, four-point bending,
// CC BY 4.0): Latin-1, a preamble of "key   value unit" lines with no
// delimiter, no units row, decimal commas in e-notation, and two candidate
// displacement columns, a named deformation and the crosshead.
static const char *kZwick =
    "braunschweig-14125/4PB_DIN14125_NFK_Osten_19122024_Hemp80_Cdry_reference_class3_1.CSV";

TS_CASE(zwick_export_reads_its_whitespace_preamble_and_columns)
{
    const auto r = readTable(fixture(kZwick));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.delimiter == ';');
    TS_CHECK(t.decimal == ',');
    TS_CHECK(t.names == QStringList({"Verformung", "Standardkraft", "Zeit", "Traversenweg",
                                     QString::fromUtf8("Prüfzeit")}));
    TS_REQUIRE(t.columns.size() == 5);
    TS_CHECK(t.columns[1].size() == 1549);
    TS_NEAR(t.columns[1][0], 0.1256414204836, 1e-15);
    TS_NEAR(*preambleQuantity(t, {"Probenbreite"}, "length"), 14.85e-3, 1e-15);
    TS_NEAR(*preambleQuantity(t, {"Probendicke"}, "length"), 2.52e-3, 1e-15);
    TS_NEAR(*preambleQuantity(t, {QString::fromUtf8("Stützweite")}, "length"), 45e-3, 1e-15);
}

TS_CASE(a_named_deformation_is_preferred_over_the_crosshead)
{
    const auto r = readTable(fixture(kZwick));
    TS_REQUIRE(r.table.has_value());
    TS_CHECK(guessForceColumn(*r.table) == "Standardkraft");
    TS_CHECK(guessExtensionColumn(*r.table) == "Verformung");
}

TS_CASE(french_compression_export_reads_quoted_names_and_units)
{
    // Universidade de Sao Paulo, PU foam to ASTM D1621 (CC BY 4.0).
    const auto r = readTable(fixture("puf-d1621/1BAT-DIR1-1.TRA"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.units == QStringList({"s", "mm", "N"}));
    TS_CHECK(guessForceColumn(t) == "Force standard");
    TS_CHECK(guessExtensionColumn(t) == QString::fromUtf8("Déformation nominale"));
    TS_CHECK(t.columns[0].size() == 3991);
}

TS_CASE(deflection_and_crosshead_columns_are_recognised)
{
    for (const char *name : {"deflection", "Durchbiegung", "compression", "Stauchung"}) {
        Table t;
        t.names = {"time", name, "force"};
        if (guessExtensionColumn(t) != name)
            ts::test::fail(__FILE__, __LINE__, std::string("not found: ") + name);
    }
    // A crosshead counts only when nothing better is there.
    Table t;
    t.names = {"time", "crosshead", "force"};
    TS_CHECK(guessExtensionColumn(t) == "crosshead");
    t.names = {"time", "crosshead", "deflection", "force"};
    TS_CHECK(guessExtensionColumn(t) == "deflection");
    t.names = {"time", "deflection", "Durchbiegung", "force"};
    QString why;
    TS_CHECK(guessExtensionColumn(t, &why).isEmpty());
    TS_CHECK(why.contains("2"));
}

// --- Instron's exports (tests/fixtures/instron/SOURCE.md) -----------------------
// Real files from Bluehill 2/3, Bluehill Universal and Merlin, one per
// dialect, each with a property checked by hand against its own data.

static QString instron(const char *name) { return fixture((std::string("instron/") + name).c_str()); }

static bool hasPreambleKey(const Table &t, const QString &key)
{
    for (const auto &p : t.preamble)
        if (p.key == key)
            return true;
    return false;
}

TS_CASE(bluehill_flexure_export_reads_imperial_units_and_its_header_result)
{
    // Bluehill's header: "Maximum Load : Maximum Load","131.18347",lbf and
    // the extension there, 1.50294 in. Its curve's maximum is that row.
    const auto r = readTable(instron("bluehill2-flexure.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.names == QStringList({"Time", "Extension", "Load"}));
    TS_CHECK(t.units == QStringList({"s", "in", "lbf"}));
    TS_CHECK(guessForceColumn(t) == "Load");
    TS_CHECK(guessExtensionColumn(t) == "Extension");
    // A key is found behind Bluehill's "Section : " prefix.
    TS_NEAR(preambleQuantity(t, {"Maximum Load"}, "force").value_or(NAN), 131.18347 * 4.4482216152605,
            1e-9);

    TensionMapping m;
    m.needsGeometry = false;
    const auto imp = toTension(t, m);
    TS_REQUIRE(imp.error.isEmpty());
    std::size_t k = 0;
    for (std::size_t i = 0; i < imp.input.force.size(); ++i)
        if (imp.input.force[i] > imp.input.force[k])
            k = i;
    TS_NEAR(imp.input.force[k], 131.18347 * 4.4482216152605, 1e-9);
    TS_NEAR(imp.input.extension[k], 1.50294 * 0.0254, 1e-12);
}

TS_CASE(bluehill_universal_results_table_is_kept_as_the_files_own_results)
{
    const auto r = readTable(instron("bluehill-universal-results.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    auto find = [&](const QString &name) -> const FileResult * {
        for (const FileResult &f : t.results)
            if (f.name == name)
                return &f;
        return nullptr;
    };
    const FileResult *fm = find("Maximum Force");
    TS_REQUIRE(fm != nullptr);
    TS_CHECK(fm->table == "Results Table 1");
    TS_CHECK(fm->row == "1");
    TS_CHECK(fm->unit == "N");
    TS_CHECK(fm->value == "285.30");
    const FileResult *e = find("Modulus (Automatic Young's)");
    TS_REQUIRE(e != nullptr);
    TS_CHECK(e->unit == "MPa" && e->value == "143.72");
    const FileResult *nu = find("Poisson's ratio (Chord)");
    TS_REQUIRE(nu != nullptr);
    TS_CHECK(nu->value == "-----");  // kept as written: not a number, not a zero
    TS_NEAR(fileResultQuantity(t, {"Maximum Force"}, "force").value_or(NAN), 285.30, 1e-9);
    TS_CHECK(!fileResultQuantity(t, {"Poisson's ratio (Chord)"}, "strain"));

    // The results row is not header data, and the curve below is read whole.
    TS_CHECK(!hasPreambleKey(t, "1"));
    TS_CHECK(t.names == QStringList({"Time", "Displacement", "Force", "Tensile displacement",
                                     "Tensile strain (Displacement)", "Tensile stress"}));
    TS_CHECK(t.units == QStringList({"s", "mm", "kN", "mm", "%", "MPa"}));
    // Bluehill's leading column is empty in every row: left out, and said.
    TS_CHECK(!t.notes.filter("empty").isEmpty());
    TS_CHECK(t.columns[0].size() == 340);
    // Bluehill's maximum force is on the curve: 0.2853 kN at 6.74 s.
    std::size_t k = 0;
    for (std::size_t i = 0; i < t.columns[2].size(); ++i)
        if (t.columns[2][i] > t.columns[2][k])
            k = i;
    TS_NEAR(t.columns[2][k], 0.2853, 1e-12);
    TS_NEAR(t.columns[0][k], 6.74, 1e-12);
}

TS_CASE(bluehill_universal_header_with_the_unit_before_the_value)
{
    // "Specimen properties : Width,(in),"0.39"": unit, then value.
    const auto r = readTable(instron("bluehill-universal-composite.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_NEAR(preambleQuantity(t, {"Width"}, "length").value_or(NAN), 0.39 * 0.0254, 1e-12);
    TS_NEAR(preambleQuantity(t, {"Thickness"}, "length").value_or(NAN), 0.04 * 0.0254, 1e-12);
    TS_NEAR(fileResultQuantity(t, {"Area"}, "area").value_or(NAN), 30.42e-6, 1e-15);
    TS_CHECK(t.names == QStringList({"Time", "Displacement", "Force", "Bonded Strain Gage",
                                     "Tensile stress"}));
    TS_CHECK(t.columns[0].size() == 3682);
}

TS_CASE(italian_bluehill_export_is_read_by_its_column_names)
{
    const auto r = readTable(instron("bluehill2-italian.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.delimiter == ';');
    TS_CHECK(t.decimal == '.');
    TS_CHECK(t.names == QStringList({"Tempo", "Corsa", "Carico"}));
    TS_CHECK(guessForceColumn(t) == "Carico");
    TS_CHECK(guessExtensionColumn(t) == "Corsa");
}

TS_CASE(merlin_export_states_units_inside_its_column_names)
{
    const auto r = readTable(instron("merlin-compression.raw"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(columnUnit(t, "Time sec") == "sec");
    TS_CHECK(columnUnit(t, "Extension mm") == "mm");
    TS_CHECK(columnUnit(t, "Load N") == "N");
    TS_CHECK(columnUnit(t, "Strain [Exten.] %") == "%");
    TS_CHECK(columnUnit(t, "Cycle Count").isEmpty());
    TS_CHECK(guessForceColumn(t) == "Load N");
    // "Area:",9.677400,"mm*mm"
    TS_NEAR(preambleQuantity(t, {"Area"}, "area").value_or(NAN), 9.6774e-6, 1e-15);
    TensionMapping m;
    m.force = "Load N";
    m.extension = "Extension mm";
    m.needsGeometry = false;
    const auto imp = toTension(t, m);
    TS_REQUIRE(imp.error.isEmpty());
    TS_CHECK(imp.forceUnit == "N" && imp.extensionUnit == "mm");
    TS_NEAR(imp.input.force[0], -2.130933, 1e-12);
}

TS_CASE(bluehill_dimension_header_gives_the_area)
{
    const auto r = readTable(instron("bluehill2-dimensions.csv"));
    TS_REQUIRE(r.table.has_value());
    TS_NEAR(preambleQuantity(*r.table, {"Area"}, "area").value_or(NAN), 181.1716e-6, 1e-15);
}

TS_CASE(force_and_displacement_names_in_other_languages_are_recognised)
{
    const std::vector<std::pair<const char *, const char *>> pairs{
        {"Carico", "Allungamento"}, {"Forza", "Deformazione"}, {"Fuerza", "Alargamiento"},
        {"Carga", "Deformación"},   {"Força", "Deformação"}};
    for (const auto &[f, e] : pairs) {
        Table t;
        t.names = {"time", QString::fromUtf8(f), QString::fromUtf8(e)};
        if (guessForceColumn(t) != QString::fromUtf8(f))
            ts::test::fail(__FILE__, __LINE__, std::string("force not found: ") + f);
        if (guessExtensionColumn(t) != QString::fromUtf8(e))
            ts::test::fail(__FILE__, __LINE__, std::string("extension not found: ") + e);
    }
    // A crosshead in Italian, Spanish and Portuguese counts only when nothing
    // better is there.
    for (const char *c : {"Corsa", "Desplazamiento", "Deslocamento"}) {
        Table t;
        t.names = {"time", c, "Carico"};
        if (guessExtensionColumn(t) != c)
            ts::test::fail(__FILE__, __LINE__, std::string("crosshead not found: ") + c);
    }
}

// --- MTS ---------------------------------------------------------------------
// Real exports, unchanged (tests/fixtures/mts/SOURCE.md). Every expected value
// below was read from the file with awk, not with TenSense.

// The value of the preamble entry whose key, without a trailing colon, is key.
static QString entry(const Table &t, const QString &key)
{
    for (const auto &p : t.preamble) {
        QString k = p.key.trimmed();
        if (k.endsWith(':'))
            k.chop(1);
        if (k.trimmed() == key)
            return p.value;
    }
    return "(none)";
}

TS_CASE(mts793_export_written_in_chunks_is_read_whole_and_in_order)
{
    // MTS 793 writes its data in blocks of 1024 rows, each under the same
    // header again; reading only the first block silently lost 221 rows.
    const auto r = readTable(fixture("mts/mts793-chunked.dat"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.delimiter == '\t');
    TS_CHECK(t.decimal == '.');
    TS_CHECK(t.names == QStringList({"Time", "Ch 1 LVDT", "Ch 1 Force"}));
    TS_CHECK(t.units == QStringList({"s", "mm", "N"}));
    TS_REQUIRE(t.columns.size() == 3);
    TS_CHECK(t.columns[0].size() == 1245);
    TS_NEAR(t.columns[0][0], 0.11035156, 1e-12);
    TS_NEAR(t.columns[0][1023], 102.01074, 1e-12);  // the first block's last row
    TS_NEAR(t.columns[0][1024], 102.11035, 1e-12);  // the second block's first
    TS_NEAR(t.columns[2][1244], -0.22730324, 1e-12);
    TS_CHECK(t.results.empty());
    TS_CHECK(t.notes.join("\n").contains("2 blocks"));
    TS_CHECK(guessForceColumn(t) == "Ch 1 Force");
    TS_CHECK(guessExtensionColumn(t) == "Ch 1 LVDT");
}

TS_CASE(german_mts793_export_keeps_its_other_block_as_the_files_own_values)
{
    // Latin-1, decimal comma, an operator block of key-value lines, the data,
    // then a second block of other columns and two rows.
    const auto r = readTable(fixture("mts/mts793-german.dat"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.decimal == ',');
    TS_CHECK(t.names == QStringList({"Datum", "Laufzeit", "Ch 1 Kraft Sollwert", "Ch 1 Kraft",
                                     "Ch 1 Weg", "1944 B", "1281068", "2157"}));
    TS_CHECK(t.units == QStringList({"d", "s", "kN", "kN", "mm", "mm", "mm", "mm"}));
    TS_REQUIRE(t.columns.size() == 8);
    TS_CHECK(t.columns[3].size() == 572);
    TS_NEAR(t.columns[3][0], -1.0232348, 1e-12);
    TS_CHECK(entry(t, QString::fromUtf8("Prüfer")) == "Kh");
    TS_CHECK(entry(t, "Durchmesser") == "98,6");
    // The second block: 5 columns by 2 rows, values as written.
    TS_CHECK(t.results.size() == 10);
    const FileResult *low = nullptr;
    for (const auto &f : t.results)
        if (f.name == "Ch 1 Kraft" && f.row == "2")
            low = &f;
    TS_REQUIRE(low != nullptr);
    TS_CHECK(low->value == "-147,20456");
    TS_CHECK(low->unit == "kN");
    TS_CHECK(low->table.startsWith("Datenerfassung"));
    // The set point is a command, not a measured force.
    TS_CHECK(guessForceColumn(t) == "Ch 1 Kraft");
    TS_CHECK(guessExtensionColumn(t) == "Ch 1 Weg");
}

TS_CASE(testsuite_export_reads_its_key_colon_value_header)
{
    const auto r = readTable(fixture("mts/testsuite-quoted.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.delimiter == ',');
    TS_CHECK(t.names == QStringList({"Crosshead", "Load", "Time"}));
    TS_CHECK(t.units == QStringList({"mm", "N", "sec"}));
    TS_CHECK(t.columns[0].size() == 681);
    TS_NEAR(t.columns[1][680], -85.9580993652344, 1e-12);
    QStringList keys;
    for (const auto &p : t.preamble)
        keys << p.key;
    TS_CHECK(keys == QStringList({"File Path", "Test", "Test Run", "Date"}));
    TS_CHECK(entry(t, "Test Run") == "BMA_01TS55_01_20211122");
    TS_CHECK(guessForceColumn(t) == "Load");
    TS_CHECK(guessExtensionColumn(t) == "Crosshead");
}

TS_CASE(testsuite_export_with_a_decimal_comma_and_exponents)
{
    const auto r = readTable(fixture("mts/testsuite-decimal-comma.txt"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.delimiter == '\t');
    TS_CHECK(t.decimal == ',');
    TS_CHECK(t.names == QStringList({"CycleCount", "Axial Displacement", "Axial Force"}));
    TS_CHECK(t.columns[1].size() == 1856);
    TS_NEAR(t.columns[1][0], 6.97374318292532e-05, 1e-18);
    TS_NEAR(t.columns[2][1], 2.47375440597534, 1e-12);
    TS_CHECK(entry(t, "Test") == "Quasi static tensile");
    TS_CHECK(guessExtensionColumn(t) == "Axial Displacement");
}

TS_CASE(french_testsuite_export_asks_which_of_its_deformations_to_use)
{
    // Two extensometer readings and the crosshead's deformation: TenSense
    // cannot know which the person wants, so it asks.
    const auto r = readTable(fixture("mts/testsuite-french.txt"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.decimal == '.');
    TS_CHECK(t.columns[0].size() == 120);
    TS_CHECK(t.names.value(3) == QString::fromUtf8("Déformation_Traverse"));
    TS_CHECK(entry(t, "Essai") == "Traction_MOE_Dopamics");
    TS_CHECK(guessForceColumn(t) == "Force");
    TS_CHECK(guessExtensionColumn(t).isEmpty());
    const TensionImport ti = toTension(t, TensionMapping{});
    TS_CHECK(ti.error.contains("3 candidates"));
}

// --- Shimadzu ------------------------------------------------------------------
// Real exports, unchanged (tests/fixtures/shimadzu/SOURCE.md); expected values
// read with iconv and awk.

TS_CASE(japanese_trapezium_export_is_read_from_shift_jis)
{
    // Shift-JIS, half-width katakana in the names, a trailing comma on every
    // line (an empty last column), ".335" with no leading zero.
    const auto r = readTable(fixture("shimadzu/trapezium-japanese.csv"));
    TS_REQUIRE(r.table.has_value());
    const Table &t = *r.table;
    TS_CHECK(t.names == QStringList({QString::fromUtf8("時間"), QString::fromUtf8("試験力"),
                                     QString::fromUtf8("ｽﾄﾛｰｸ")}));
    TS_CHECK(t.units == QStringList({"sec", "N", "mm"}));
    TS_REQUIRE(t.columns.size() == 3);
    TS_CHECK(t.columns[1].size() == 2012);
    TS_NEAR(t.columns[1][0], 3.125, 1e-15);
    TS_NEAR(t.columns[2][2011], 0.335, 1e-15);
    TS_CHECK(entry(t, QString::fromUtf8("試験名")) == QString::fromUtf8("試験ﾓｰﾄﾞ"));
    TS_CHECK(t.notes.join("\n").contains("Shift-JIS"));
    TS_CHECK(guessForceColumn(t) == QString::fromUtf8("試験力"));
    TS_CHECK(guessExtensionColumn(t) == QString::fromUtf8("ｽﾄﾛｰｸ"));
}

TS_CASE(specimens_side_by_side_are_read_one_at_a_time)
{
    const QString path = fixture("shimadzu/trapezium-side-by-side.csv");
    const QStringList six{"Time", "Force", "Stroke", "Force", "Stress", "Strain"};
    const auto first = readTable(path);
    TS_REQUIRE(first.table.has_value());
    TS_CHECK(first.table->fileSpecimens == QStringList({"1- 1", "1- 2", "1- 3"}));
    TS_CHECK(first.table->fileSpecimen == "1- 1");
    TS_CHECK(first.table->names == six);
    TS_CHECK(first.table->units == QStringList({"sec", "kN", "mm", "N", "N/mm2", "%"}));
    // Each specimen's own rows, its blank cells below its end left out.
    TS_CHECK(first.table->columns[0].size() == 1724);
    TS_NEAR(first.table->columns[0].back(), 86.15, 1e-12);
    TS_CHECK(first.table->notes.join("\n").contains("3 specimens"));

    const auto second = readTable(path, "1- 2");
    TS_REQUIRE(second.table.has_value());
    TS_CHECK(second.table->fileSpecimen == "1- 2");
    TS_CHECK(second.table->names == six);
    TS_CHECK(second.table->columns[0].size() == 4579);
    TS_NEAR(second.table->columns[4].back(), 0.273284314, 1e-12);

    const auto third = readTable(path, "1- 3");
    TS_REQUIRE(third.table.has_value());
    TS_CHECK(third.table->columns[0].size() == 1690);
    TS_NEAR(third.table->columns[5].back(), 0.039390486, 1e-12);

    const auto none = readTable(path, "2- 1");
    TS_CHECK(!none.table.has_value());
    TS_CHECK(none.error.contains("1- 1, 1- 2, 1- 3"));
}

TS_CASE(two_columns_of_one_name_are_one_choice)
{
    // "Force" in kN and again in N: the same quantity, so not a question;
    // the first is the one a name finds.
    const auto r = readTable(fixture("shimadzu/trapezium-side-by-side.csv"));
    TS_REQUIRE(r.table.has_value());
    TS_CHECK(guessForceColumn(*r.table) == "Force");
    TS_CHECK(columnUnit(*r.table, "Force") == "kN");
}

TS_CASE(a_file_of_one_specimen_names_none)
{
    const auto r = readTable(fixture("mts/testsuite-quoted.csv"));
    TS_REQUIRE(r.table.has_value());
    TS_CHECK(r.table->fileSpecimens.isEmpty());
    TS_CHECK(r.table->fileSpecimen.isEmpty());
}

int main() { return ts::test::run(); }
