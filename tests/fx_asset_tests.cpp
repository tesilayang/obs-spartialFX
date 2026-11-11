#include "fx_catalog.hpp"
#include <QCoreApplication>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;

void check(bool ok, const std::string &label)
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
    if (!ok)
        ++failures;
}

void fixture(bool ok, const char *label)
{
    if (!ok)
        throw std::runtime_error(label);
}

spatial::FxInstance load(const QString &path, spatial::FxAsset kind)
{
    spatial::FxInstance instance;
    instance.path = path.toStdString();
    spatial::loadFxAsset(instance, kind);
    return instance;
}

void writeFile(const QString &path, const QString &contents)
{
    QFile file(path);
    fixture(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open cube fixture");
    const auto bytes = contents.toUtf8();
    fixture(file.write(bytes) == bytes.size(), "write cube fixture");
}

const QString identityRows = QStringLiteral(
    "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
    "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");

bool rejected(const spatial::FxInstance &instance)
{
    return !instance.pixels && !instance.assetError.empty();
}

void cacheKindsValidateIndependently(const QTemporaryDir &directory)
{
    QImage ordinary(7, 5, QImage::Format_RGBA8888);
    ordinary.fill(QColor(180, 90, 40, 255));
    const auto ordinaryPath = directory.filePath("ordinary.png");
    fixture(ordinary.save(ordinaryPath, "PNG"), "save ordinary PNG fixture");

    // Keep the first decoded pixels alive, so a weak cache hit really occurs.
    const auto image = load(ordinaryPath, spatial::FxAsset::Image);
    fixture(bool(image.pixels), "load ordinary image fixture");
    const auto invalidLut = load(ordinaryPath, spatial::FxAsset::Lut);
    check(rejected(invalidLut), "Image then Lut independently rejects a non-LUT PNG");
    const auto sameImage = load(ordinaryPath, spatial::FxAsset::Image);
    check(sameImage.pixels == image.pixels, "same-kind immutable image cache remains shared");

    QImage flattened(4, 2, QImage::Format_RGBA8888);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 4; ++x)
            flattened.setPixelColor(x, y, QColor((x % 2) * 255, y * 255, (x / 2) * 255, 255));
    const auto flattenedPath = directory.filePath("identity-2.png");
    fixture(flattened.save(flattenedPath, "PNG"), "save PNG LUT fixture");
    const auto pngImage = load(flattenedPath, spatial::FxAsset::Image);
    fixture(bool(pngImage.pixels), "load flattened PNG as image fixture");
    const auto pngLut = load(flattenedPath, spatial::FxAsset::Lut);
    check(pngLut.pixels && pngLut.assetError.empty() && pngLut.pixels->lutSize == 2,
          "Image then Lut computes PNG LUT dimension independently");
    check(pngLut.pixels && pngLut.pixels != pngImage.pixels && pngImage.pixels->lutSize == 0,
          "image and LUT metadata never share a decoded cache entry");
    const auto sameLut = load(flattenedPath, spatial::FxAsset::Lut);
    check(sameLut.pixels == pngLut.pixels, "same-kind immutable LUT cache remains shared");

    const auto reversePath = directory.filePath("identity-2-reverse.png");
    fixture(flattened.save(reversePath, "PNG"), "save reverse PNG LUT fixture");
    const auto lutFirst = load(reversePath, spatial::FxAsset::Lut);
    const auto imageSecond = load(reversePath, spatial::FxAsset::Image);
    check(lutFirst.pixels && lutFirst.pixels->lutSize == 2 && imageSecond.pixels &&
              imageSecond.pixels->lutSize == 0 && imageSecond.pixels != lutFirst.pixels,
          "Lut then Image independently resets LUT-specific metadata");

    auto recovered = invalidLut;
    recovered.path = flattenedPath.toStdString();
    spatial::loadFxAsset(recovered, spatial::FxAsset::Lut);
    check(recovered.pixels && recovered.pixels->lutSize == 2 && recovered.assetError.empty(),
          "successful cached load clears a previous asset error");
}

void cubeDomainsAndValuesAreValidated(const QTemporaryDir &directory)
{
    const auto validPath = directory.filePath("valid-domain.cube");
    writeFile(validPath, "TITLE \"domain fixture\"\nLUT_3D_SIZE 2\n"
                        "DOMAIN_MIN -1 -2 -3\nDOMAIN_MAX 2 3 4\n" + identityRows);
    const auto valid = load(validPath, spatial::FxAsset::Lut);
    check(valid.pixels && valid.assetError.empty() && valid.pixels->lutSize == 2 &&
              valid.pixels->minimum.x == -1 && valid.pixels->minimum.y == -2 &&
              valid.pixels->minimum.z == -3 && valid.pixels->maximum.x == 2 &&
              valid.pixels->maximum.y == 3 && valid.pixels->maximum.z == 4,
          "finite non-default cube domains survive decoding");
    if (valid.pixels) {
        const auto &pixels = *valid.pixels;
        check(pixels.width == 4 && pixels.height == 2 && pixels.pixels.size() == 32 &&
                  pixels.pixels[8] == 0 && pixels.pixels[9] == 0 && pixels.pixels[10] == 255 &&
                  pixels.pixels[20] == 255 && pixels.pixels[21] == 255 && pixels.pixels[22] == 0,
              "cube red-fastest data maps to horizontal blue tiles");
    }

    const QString tokens[] = {"garbage", "nan", "inf", "-inf"};
    int index = 0;
    for (const bool minimum : {true, false}) {
        for (const auto &token : tokens) {
            const auto path = directory.filePath(QString("bad-domain-%1.cube").arg(index++));
            const auto minLine = minimum ? "DOMAIN_MIN " + token + " -1 -1\n" : QString("DOMAIN_MIN -1 -1 -1\n");
            const auto maxLine = minimum ? QString("DOMAIN_MAX 2 2 2\n") : "DOMAIN_MAX " + token + " 2 2\n";
            writeFile(path, "LUT_3D_SIZE 2\n" + minLine + maxLine + identityRows);
            check(rejected(load(path, spatial::FxAsset::Lut)),
                  std::string("reject ") + (minimum ? "DOMAIN_MIN " : "DOMAIN_MAX ") + token.toStdString());
        }
    }

    const QString headers[] = {
        "LUT_3D_SIZE 2\nDOMAIN_MIN 1 0 0\nDOMAIN_MAX 1 1 1\n",
        "LUT_3D_SIZE 2\nDOMAIN_MIN 2 0 0\nDOMAIN_MAX 1 1 1\n",
        "LUT_3D_SIZE 2\nDOMAIN_MIN 0 0\n",
        "LUT_3D_SIZE 2 trailing\n",
        "LUT_3D_SIZE 2\nLUT_1D_SIZE 2\n"
    };
    for (const auto &header : headers) {
        const auto path = directory.filePath(QString("bad-header-%1.cube").arg(index++));
        writeFile(path, header + identityRows);
        check(rejected(load(path, spatial::FxAsset::Lut)), "reject malformed cube header " + std::to_string(index));
    }

    const auto badRow = directory.filePath("nonfinite-row.cube");
    writeFile(badRow, "LUT_3D_SIZE 2\nnan 0 0\n" + identityRows.mid(identityRows.indexOf('\n') + 1));
    check(rejected(load(badRow, spatial::FxAsset::Lut)), "reject non-finite cube sample values");
    const auto truncated = directory.filePath("truncated.cube");
    writeFile(truncated, "LUT_3D_SIZE 2\n0 0 0\n");
    check(rejected(load(truncated, spatial::FxAsset::Lut)), "reject incomplete cube data");
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    try {
        QTemporaryDir directory;
        fixture(directory.isValid(), "create temporary fixture directory");
        cacheKindsValidateIndependently(directory);
        cubeDomainsAndValuesAreValidated(directory);
    } catch (const std::exception &error) {
        std::cerr << "Fixture error: " << error.what() << '\n';
        return 2;
    }
    std::cout << "Asset regressions: " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
