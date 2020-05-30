#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QVector>
#include <QBuffer>
#include <QFileDialog>
#include <QtDebug>

#include <QStandardPaths>
#include <QProgressDialog>
#include <QErrorMessage>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include "tinyxml2.h"
#include "wwriff.h"
#include <QMainWindow>






typedef uint32_t UInt32;
typedef uint16_t UInt16;
typedef UInt32 MediaID;
typedef UInt32 Fourcc;
constexpr UInt32 BankHeaderChunkID = 'DHKB';
constexpr UInt32 BankDataIndexChunkID = 'XDID';
constexpr UInt32 BankDataChunkID = 'ATAD';
constexpr Fourcc RIFFChunkId = 'FFIR';
constexpr Fourcc WAVEChunkId = 'EVAW';
constexpr Fourcc fmtChunkId = ' tmf';
constexpr Fourcc dataChunkId = 'atad';



struct Sound
{
    std::string id;
    std::string name;
    std::string relativePath;
    std::string bankPath;
    bool streamed;
};


#pragma pack(push,1)
struct SubchunkHeader
{
    UInt32 dwTag;
    UInt32 dwChunkSize;
};

struct BankHeader
{
    UInt32 dwBankGeneratorVersion;
    UInt32 dwSoundBankID;
    UInt32 dwLanguageID;
    UInt16 bFeedbackInBank;
    UInt16 bDeviceAllocated;
    UInt32 dwProjectID;
};

struct MediaHeader
{
    MediaID id;
    UInt32 uOffset;
    UInt32 uSize;
};

struct ChunkHeader
{
    Fourcc ChunkId;
    UInt32 dwChunkSize;
    friend inline QDataStream &operator>>(QDataStream& in,ChunkHeader& item) {
        in >> item.ChunkId;
        in >> item.dwChunkSize;
        return in;
    }
    friend inline QDataStream &operator<<(QDataStream& out, ChunkHeader& item) {
        out << item.ChunkId;
        out << item.dwChunkSize;
        return out;
    }
};
struct WaveFormatEx
{
    UInt16 wFormatTag;
    UInt16 nChannels;
    UInt32 nSamplesPerSec;
    UInt32 nAvgBytesPerSec;
    UInt16 nBlockAlign;
    UInt16 wBitsPerSample;
    UInt16 cbSize;

    friend inline QDataStream &operator>>(QDataStream& in,WaveFormatEx& item) {
        in >> item.wFormatTag;
        in >> item.nChannels;
        in >> item.nSamplesPerSec;
        in >> item.nAvgBytesPerSec;
        in >> item.nBlockAlign;
        in >> item.wBitsPerSample;
        in >> item.cbSize;
        return in;

    }
    friend inline QDataStream &operator<<(QDataStream& out,WaveFormatEx& item) {
        out << item.wFormatTag;
        out << item.nChannels;
        out << item.nSamplesPerSec;
        out << item.nAvgBytesPerSec;
        out << item.nBlockAlign;
        out << item.wBitsPerSample;
        out << item.cbSize;
        return out;

    }
};

struct WaveFormatExtensible : public WaveFormatEx
{
    UInt16 wSamplesPerBlock;
    UInt32 dwChannelMask;

    friend inline QDataStream &operator>>(QDataStream& in,WaveFormatExtensible& item) {
        in >> (WaveFormatEx &)item;
        in >> item.wSamplesPerBlock;
        in >> item.dwChannelMask;
        return in;
    }
    friend inline QDataStream &operator<<(QDataStream& out,WaveFormatExtensible& item) {
        out << (WaveFormatEx &)item;
        out << item.wSamplesPerBlock;
        out << item.dwChannelMask;
        return out;
    }
};

struct VorbisHeaderBase
{
    UInt32 dwTotalPCMFrames;
};

struct VorbisLoopInfo
{
    UInt32 dwLoopStartPacketOffset;
    UInt32 dwLoopEndPacketOffset;
    UInt16 uLoopBeginExtra;
    UInt16 uLoopEndExtra;
};

struct VorbisInfo
{
    VorbisLoopInfo LoopInfo;
    UInt32 dwSeekTableSize;
    UInt32 dwVorbisDataOffset;
    UInt16 uMaxPacketSize;
    UInt16 uLastGranuleExtra;
    UInt32 dwDecodeAllocSize;
    UInt32 dwDecodeX64AllocSize;
    UInt32 uHashCodebook;
    unsigned __int8 uBlockSizes[2];
};
struct VorbisHeader : public VorbisHeaderBase, public VorbisInfo
{
};
#pragma pack(pop)

//Implementation of struct deserialization code







QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_openButton_clicked();

    void on_extractButton_clicked();

private:
    Ui::MainWindow *ui;
    QVector<Sound> savedSounds;

};
#endif // MAINWINDOW_H
