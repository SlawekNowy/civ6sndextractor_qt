/*
soundextract
Copyright 2018 Jonathan wilson
Copyright 2020 Sławomir Śpiewak

Soundextract is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version. See the file COPYING for more details.
*/

/* WWISE ADPCM decode algorithm and related information taken from reformat.c (copyright for that is given below)

The MIT License (MIT)

Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


//TODO USe wxWidgets or QT for GUI?
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "soundextract.h"
#include "ui_soundextract.h"


char *datachunk;
std::string path;
std::vector<MediaHeader> media;

std::vector<Sound> sounds;

void LoadBank(std::string fname)
{
    FILE *f = fopen(fname.c_str(), "rb");
    SubchunkHeader sc;
    fread(&sc, sizeof(sc), 1, f);
    if (sc.dwTag == BankHeaderChunkID)
    {
        BankHeader h;
        fread(&h, sizeof(h), 1, f);
        fseek(f, sc.dwChunkSize - sizeof(h), SEEK_CUR);
        while (fread(&sc, sizeof(sc), 1, f))
        {
            switch (sc.dwTag)
            {
            case BankDataIndexChunkID:
                for (unsigned int i = 0; i < sc.dwChunkSize / sizeof(MediaHeader); i++)
                {
                    MediaHeader m;
                    fread(&m, sizeof(m), 1, f);
                    media.push_back(m);
                }
                break;
            case BankDataChunkID:
                datachunk = new char[sc.dwChunkSize];
                fread(datachunk, sizeof(datachunk[0]), sc.dwChunkSize, f);
                break;
            default:
                fseek(f, sc.dwChunkSize, SEEK_CUR);
                break;
            }
        }
    }
    fclose(f);
}

void ParseFiles(tinyxml2::XMLNode *xml, bool streamed)
{
    for (xml = xml->FirstChildElement("File");xml;xml = xml->NextSiblingElement("File"))
    {
        Sound sound;
        tinyxml2::XMLNode * pPrefetchSize = xml->FirstChildElement("PrefetchSize");
        if (pPrefetchSize!=nullptr) {
            continue; //this file might be abnormally cut, just don't parse it further
        }
        sound.streamed = streamed;
        sound.id = xml->ToElement()->Attribute("Id");
        xml = xml->FirstChildElement("ShortName");
        sound.name = xml->FirstChild()->ToText()->Value();
        sound.name.erase(sound.name.rfind('.'));
        xml = xml->Parent();
        xml = xml->FirstChildElement("Path");
        QFileInfo fi(xml->FirstChild()->ToText()->Value());
        sound.relativePath = QDir::fromNativeSeparators(fi.path()).toStdString();
        xml = xml->Parent();


        sounds.push_back(sound);
    }
}

int revorb(const char *fname);


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->soundWavesOpened->setSelectionMode(QAbstractItemView::MultiSelection);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_openButton_clicked()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          "Select one or More xml files",
                                                          "",
            "XML Files (*.xml)");
    //TODO progress bar
    for (auto fileName:fileNames) { //should be QString here
        fileName = QDir::fromNativeSeparators(fileName);
        //first find the bank file
        QFileInfo relFileName(fileName);
        path = relFileName.path().toStdString();
        QFileInfo relFileBank(relFileName.path()+"/"+relFileName.baseName()+".bnk");
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLNode *xml = &doc;
        doc.LoadFile(qPrintable(relFileName.absoluteFilePath()));
        xml = xml->FirstChildElement("SoundBanksInfo");
        if (!xml)
        {
            QErrorMessage eMSG(this);
            eMSG.showMessage("Cannot find necessary element. This is likely not the file I'm looking for.");
            return;
        }
        xml = xml->FirstChildElement("SoundBanks");
        if (!xml)
        {

            QErrorMessage eMSG(this);
            eMSG.showMessage("Cannot find necessary element. This is likely not the file I'm looking for.");
            return;
        }
        xml = xml->FirstChildElement("SoundBank");
        if (!xml)
        {

            QErrorMessage eMSG(this);
            eMSG.showMessage("Cannot find necessary element. This is likely not the file I'm looking for.");
            return;
        }
        bool found = false;

            if (xml->FirstChildElement("ReferencedStreamedFiles")) {
                xml = xml->FirstChildElement("ReferencedStreamedFiles");
                if (xml)
                {
                    ParseFiles(xml, true);
                    sounds.erase(std::remove_if(sounds.begin(), sounds.end(), [](const Sound & s)
                    {
                        std::string fname = path;
                        fname += L'/';
                        fname += s.id;
                        fname += ".wem";
                        QFileInfo wemFile(QString::fromStdString(fname));
                        return !wemFile.exists();
                    }
                    ), sounds.end());
                    found = true;

                xml=xml->Parent();
            }



            if (xml->FirstChildElement("IncludedMemoryFiles")) {
                xml = xml->FirstChildElement("IncludedMemoryFiles");
                if (xml)
                {
                    ParseFiles(xml, false);
                    found = true;
                }
                xml = xml->Parent();
            }



        std::sort(sounds.begin(), sounds.end(), [](Sound s1, Sound s2)
        {
            return s1.name < s2.name;
        });
        if (!found) return;
        QStringList addedWaves;

        for (auto sound:sounds) {
            sound.bankPath = relFileBank.absoluteFilePath().toStdString();
            addedWaves.append(QString::fromStdString(sound.name));
        }
        if (addedWaves.size()>0) {
            ui->soundWavesOpened->addItems(addedWaves);

            savedSounds =*(new QVector<Sound>(sounds.begin(),sounds.end()));
            std::sort(savedSounds.begin(), savedSounds.end(), [](Sound s1, Sound s2)
            {
                return s1.name < s2.name;
            });
            sounds.clear();
            sounds.shrink_to_fit();
            LoadBank(relFileBank.absoluteFilePath().toStdString());
            //just making these a bit faster.
        }
    }
}
}

void MainWindow::on_extractButton_clicked()
{

    //HWND list = GetDlgItem(hwnd, IDC_SOUNDS); //get element from GUI
    //int item = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    QStringList filesToExport;
        if (ui->soundWavesOpened->selectedItems().size()!=0) {
            sounds.reserve(ui->soundWavesOpened->selectedItems().size());
            for (auto element:ui->soundWavesOpened->selectedItems()) {
                filesToExport.append(element->text());
                QVector<Sound>::iterator it = std::find_if(savedSounds.begin(),savedSounds.end(),[element] (Sound val) {
                    return val.name == element->text().toStdString();
                });
                Q_ASSERT(it!=savedSounds.end()); //std::vector<T>.end() refers to savedSounds[n+1] (where n is vector.size()) which doesn't exist
                sounds.push_back(*it);
            }

        } else {
            sounds.reserve(savedSounds.size());
            for(int i = 0; i < ui->soundWavesOpened->count(); ++i)
            {
                QListWidgetItem* item = ui->soundWavesOpened->item(i);
                filesToExport.append(item->text());
            }
            sounds = *(new std::vector<Sound>(savedSounds.begin(),savedSounds.end()));
        }
        QString dirExport = QFileDialog::getExistingDirectory(this);

        QProgressDialog progress(this);
        progress.setLabelText("Exporting...");
        progress.setMinimum(0);
        progress.setMaximum(sounds.size());

        for (auto iterator = sounds.begin();iterator !=sounds.end();iterator++) {
            Sound sound = *iterator;
            char *outdata = nullptr;
            long size = 0;
            if (sound.streamed)
            {
                std::string infname = path;
                infname += '\\';
                infname += sound.id;
                infname += ".wem";
                FILE *infile = fopen(infname.c_str(), "rb");
                fseek(infile, 0, SEEK_END);
                size = ftell(infile);
                fseek(infile, 0, SEEK_SET);
                outdata = new char[size];
                fread(outdata, sizeof(outdata[0]), size, infile);
                fclose(infile);
            }
            else
            {
                MediaID id = stoul(sound.id);
                for (unsigned int i = 0; i < media.size(); i++)
                {
                    if (media[i].id == id)
                    {
                        size = media[i].uSize;
                        outdata = new char[size];
                        memcpy(outdata, &datachunk[media[i].uOffset], size);
                        break;
                    }
                }
            }
            char *ptr = outdata;
            if (*reinterpret_cast<Fourcc *>(ptr) != RIFFChunkId)
            {
                delete[] outdata;
                //return true;
            }
            ptr += sizeof(Fourcc);
            ptr += sizeof(UInt32);
            if (*reinterpret_cast<Fourcc *>(ptr) != WAVEChunkId)
            {
                delete[] outdata;
                //return true;
            }
            ptr += sizeof(Fourcc);
            ChunkHeader header = *reinterpret_cast<ChunkHeader *>(ptr);
            if (header.ChunkId != fmtChunkId)
            {
                delete[] outdata;
                //return true;
            }
            ptr += sizeof(ChunkHeader);
            WaveFormatExtensible format = *reinterpret_cast<WaveFormatExtensible *>(ptr);
            ptr += sizeof(WaveFormatExtensible);
            std::string ext;
            if (format.wFormatTag == 2)
            {
                if (header.dwChunkSize != sizeof(WaveFormatExtensible))
                {
                    delete[] outdata;
                    //return true;
                }
                ext = ".wav";
            }
            else if (format.wFormatTag == 0xFFFE)
            {
                if (header.dwChunkSize != sizeof(WaveFormatExtensible))
                {
                    delete[] outdata;
                    //return true;
                }
                ext = ".wav";
            }
            else if (format.wFormatTag == 0xFFFF)
            {
                if (header.dwChunkSize != sizeof(WaveFormatExtensible) + sizeof(VorbisHeader))
                {
                    delete[] outdata;
                    //return true;
                }
                ext = ".ogg";
            }
            //if (GetSaveFileName(&of))
            QFileInfo outFile(dirExport+"/"+QString::fromStdString(sound.relativePath)+"/"+QString::fromStdString(sound.name)+QString::fromStdString(ext));
                    QDir dir;
                    if (!dir.exists(dirExport+"/"+QString::fromStdString(sound.relativePath)+"/"))
                        dir.mkpath(dirExport+"/"+QString::fromStdString(sound.relativePath)+"/"); // You can check the success if needed

                    QString qstring = outFile.filePath();
                    std::string std_string = qstring.toStdString();
                    const char* lBuf = std_string.c_str();
                    {
                if (format.wFormatTag == 2)
                {
                    format.wFormatTag = 0x11;
                    format.wSamplesPerBlock = (format.nBlockAlign - 4 * format.nChannels) * 8 / (format.wBitsPerSample * format.nChannels) + 1;
                    char *datapos = nullptr;
                    UInt32 datasize = 0;
                    while (ptr < outdata + size)
                    {
                        header = *reinterpret_cast<ChunkHeader *>(ptr);
                        ptr += sizeof(ChunkHeader);
                        if (header.ChunkId == dataChunkId)
                        {
                            datapos = ptr;
                            datasize = header.dwChunkSize;
                        }
                        ptr += header.dwChunkSize;
                    }
                    if (!datapos || !datasize)
                    {
                        delete[] outdata;
                        //return true;
                    }
                    FILE *outfile = fopen(lBuf, "wb");
                    header.ChunkId = RIFFChunkId;
                    header.dwChunkSize = sizeof(Fourcc) + sizeof(ChunkHeader) + sizeof(WaveFormatExtensible) + sizeof(ChunkHeader) + datasize;
                    fwrite(&header, sizeof(header), 1, outfile);
                    Fourcc fcc = WAVEChunkId;
                    fwrite(&fcc, sizeof(fcc), 1, outfile);
                    header.ChunkId = fmtChunkId;
                    header.dwChunkSize = sizeof(WaveFormatExtensible);
                    fwrite(&header, sizeof(header), 1, outfile);
                    fwrite(&format, sizeof(format), 1, outfile);
                    header.ChunkId = dataChunkId;
                    header.dwChunkSize = datasize;
                    fwrite(&header, sizeof(header), 1, outfile);
                    if (format.nChannels > 1)
                    {
                        ptr = datapos;
                        static uint8_t transformInData[BUFSIZ], transformOutData[BUFSIZ];
                        uint8_t *transformIn = transformInData,
                            *transformOut = transformOutData;
                        size_t transformCount = BUFSIZ / format.nBlockAlign;
                        if (format.nBlockAlign > BUFSIZ)
                        {
                            transformIn = new uint8_t[2 * format.nBlockAlign];
                            transformOut = transformIn + format.nBlockAlign;
                            transformCount = 1;
                        }
                        for (size_t blockCount = 0; blockCount * format.nBlockAlign < datasize;)
                        {
                            size_t sz = format.nBlockAlign * transformCount;
                            if (datapos + datasize < ptr + sz)
                            {
                                sz = datapos + datasize - ptr;
                            }
                            memcpy(transformIn, ptr, sz);
                            ptr += sz;
                            size_t blockAmount = sz / format.nBlockAlign;
                            blockCount += blockAmount;
                            for (size_t block = 0; block < blockAmount; block++)
                            {
                                for (size_t n = 0; n < format.nBlockAlign / (format.nChannels * 4u); n++)
                                {
                                    for (size_t s = 0; s < format.nChannels; s++)
                                    {
                                        reinterpret_cast<uint32_t *>(transformOut + block * format.nBlockAlign)[n * format.nChannels + s] = reinterpret_cast<uint32_t *>(transformIn + block * format.nBlockAlign)[s * format.nBlockAlign / (format.nChannels * 4) + n];
                                    }
                                }
                            }
                            fwrite(transformOut, format.nBlockAlign, blockAmount, outfile);
                        }
                        if (format.nBlockAlign > BUFSIZ)
                        {
                            delete[] transformIn;
                        }
                    }
                    else
                    {
                        fwrite(datapos, 1, datasize, outfile);
                    }
                    fclose(outfile);
                    delete[] outdata;
                    //return true;
                }
                else if (format.wFormatTag == 0xFFFE)
                {
                    format.wFormatTag = 0x1;
                    char *datapos = nullptr;
                    UInt32 datasize = 0;
                    while (ptr < outdata + size)
                    {
                        header = *reinterpret_cast<ChunkHeader *>(ptr);
                        ptr += sizeof(ChunkHeader);
                        if (header.ChunkId == dataChunkId)
                        {
                            datapos = ptr;
                            datasize = header.dwChunkSize;
                        }
                        ptr += header.dwChunkSize;
                    }
                    if (!datapos || !datasize)
                    {
                        delete[] outdata;
                        //return true;
                    }
                    FILE *outfile = fopen(lBuf, "wb");
                    header.ChunkId = RIFFChunkId;
                    header.dwChunkSize = sizeof(Fourcc) + sizeof(ChunkHeader) + sizeof(WaveFormatExtensible) + sizeof(ChunkHeader) + datasize;
                    fwrite(&header, sizeof(header), 1, outfile);
                    Fourcc fcc = WAVEChunkId;
                    fwrite(&fcc, sizeof(fcc), 1, outfile);
                    header.ChunkId = fmtChunkId;
                    header.dwChunkSize = sizeof(WaveFormatExtensible);
                    fwrite(&header, sizeof(header), 1, outfile);
                    fwrite(&format, sizeof(format), 1, outfile);
                    header.ChunkId = dataChunkId;
                    header.dwChunkSize = datasize;
                    fwrite(&header, sizeof(header), 1, outfile);
                    fwrite(datapos, 1, datasize, outfile);
                    fclose(outfile);
                    delete[] outdata;
                    //return true;
                }
                else if (format.wFormatTag == 0xFFFF)
                {
                    if (size && outdata)
                    {
                        char *fn = tmpnam(nullptr);
                        FILE *outfile = fopen(fn, "wb");
                        fwrite(outdata, sizeof(outdata[0]), size, outfile);
                        fclose(outfile);
                        {
                            Wwise_RIFF_Vorbis ww(fn);
                            ofstream out(lBuf, ios::binary);
                            ww.generate_ogg(out);
                        }
                        _unlink(fn);
                        revorb(lBuf);
                        delete[] outdata;
                        //return true;
                    }
                }
            }

        }

}
