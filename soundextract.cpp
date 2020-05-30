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
            }


            xml = xml->FirstChildElement("IncludedMemoryFiles");
            if (xml)
            {
                ParseFiles(xml, false);
                found = true;
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

void MainWindow::on_extractButton_clicked()
{

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
        QBuffer data;
        data.open(QIODevice::ReadWrite);
        if (sound.streamed) {
            //the file is streamed. Search for coresponding wem file and copy it to memory.
            QFile file(QString::fromStdString(path)+"/"+QString::fromStdString(sound.id)+".wem");
            file.open(QIODevice::ReadOnly);
            data.write(file.readAll());
            data.seek(0);
            file.close();
        } else {
            //the file is not streamed. Search this wem from bank.
            MediaID id = std::stoul(sound.id);
            for (auto i = media.begin();i !=media.end();i++) {
                auto mediaHead = *i;
                if (mediaHead.id==id) {
                    //found one.
                    data.write(&datachunk[mediaHead.uOffset],mediaHead.uSize);
                    data.seek(0);
                }
            }
        }
        //Wem file is in memory. Sanity checks.
        QDataStream dataStr(&data);
        int testVar;
        dataStr >> testVar; //RIFF id
        if (testVar!=RIFFChunkId) {
            qWarning() << "This is not a wem file." ;
            qWarning() << "Reason: cannot find RIFF chunk";
            continue;
        }
        dataStr >> testVar; //size of WAVE chunk
        dataStr >> testVar; //WAVE id
        if (testVar!=WAVEChunkId) {

            qWarning() << "This is not a wem file.";
            qWarning() << "Reason: cannot find WAVE chunk";
            continue;
        }
        ChunkHeader header;
        dataStr >> header;
        if (header.ChunkId!=fmtChunkId) {

            qWarning() << "This is not a wem file." ;
            qWarning() << "Reason: cannot find format chunk";
            continue;
        }
        WaveFormatExtensible format;
        dataStr >> format;
        //now determine the exported format.
        QString ext;
        if (format.wFormatTag==2||format.wFormatTag==0xFFFE) {
            if (header.dwChunkSize != sizeof(WaveFormatExtensible)) {
                qWarning() << "Cannot export to .wav file";
                qWarning() << "Reason: format chunk size mismatch";
                qWarning() << "Expected:" << sizeof(WaveFormatExtensible)<<"B";
                qWarning() << "Got:" << header.dwChunkSize << "B";
                continue;
            }
            ext = ".wav";
        } else if (format.wFormatTag==0xFFFF) {
            if (header.dwChunkSize != sizeof(WaveFormatExtensible)+sizeof(VorbisHeader)) {
                qWarning() << "Cannot export to .ogg file";
                qWarning() << "Reason: format chunk size mismatch";
                qWarning() << "Expected:" << sizeof(WaveFormatExtensible)+sizeof(VorbisHeader)<<"B";
                qWarning() << "Got:" << header.dwChunkSize << "B";
                continue;
            }
            ext = ".ogg";
        }
        //determine the filename
        QFile outFile(dirExport+"/"+QString::fromStdString(sound.relativePath)+"/"+QString::fromStdString(sound.name)+ext);
        QDir dir;
        if (!dir.exists(dirExport+"/"+QString::fromStdString(sound.relativePath)+"/"))
            dir.mkpath(dirExport+"/"+QString::fromStdString(sound.relativePath)+"/"); // You can check the success if needed
        //now to actually save the file

        outFile.open(QIODevice::WriteOnly);

        if (format.wFormatTag==2) {
            format.wFormatTag = 0x11;
            format.wSamplesPerBlock = (format.nBlockAlign - 4 * format.nChannels) * 8 / (format.wBitsPerSample * format.nChannels) + 1;
            QBuffer dataChunk;
            //iterating through chunks until we find the data one.
            while (!data.atEnd()) {
                ChunkHeader header;
                dataStr >> header;
                if (header.ChunkId==dataChunkId) {
                    dataChunk.write(data.data(),header.dwChunkSize);
                }
                    data.seek(data.pos()+header.dwChunkSize);

            }
            if (dataChunk.size()==0) {
                continue;
            }
            QDataStream fileStream(&outFile);
            //write the wav file
            //Riff chunk
            header.ChunkId = RIFFChunkId;
            header.dwChunkSize = sizeof(Fourcc) + sizeof(ChunkHeader) + sizeof(WaveFormatExtensible) + sizeof(ChunkHeader) + dataChunk.size();
            fileStream << header;
            outFile.flush();
            //fwrite(&header, sizeof(header), 1, outfile);
            //WAVE chunk
            fileStream << WAVEChunkId;
            outFile.flush();
            //fwrite(&fcc, sizeof(fcc), 1, outfile);
            //fmt chunk
            header.ChunkId = fmtChunkId;
            header.dwChunkSize = sizeof(WaveFormatExtensible);
            fileStream << header;
            fileStream << format;
            outFile.flush();
            //fwrite(&header, sizeof(header), 1, outfile);
            //fwrite(&format, sizeof(format), 1, outfile);
            //data chnuk
            header.ChunkId = dataChunkId;
            header.dwChunkSize = dataChunk.size();
            fileStream << header;
            outFile.flush();
            //fwrite(&header, sizeof(header), 1, outfile);
            if (format.nChannels > 1) {
                //512 was originally BUFSIZ. Check if it works with bigger buffers first.
                //linux's BUFSIZ is 1024
                QByteArray transformIn(nullptr,512),transformOut(nullptr,512);
                size_t transformCount = 512/format.nBlockAlign;
                if (format.nBlockAlign>512) {
                    transformIn = *(new QByteArray(nullptr,format.nBlockAlign));
                    transformOut = *(new QByteArray(nullptr,format.nBlockAlign));
                    transformCount =1;
                }

                for (size_t blockCount=0;blockCount* (size_t)format.nBlockAlign<dataChunk.size();) {
                    size_t sz = format.nBlockAlign*transformCount;
                    if (dataChunk.size()<dataChunk.pos()+sz) {
                        sz = dataChunk.size() - dataChunk.pos();
                    }
                    transformIn.insert(sz,dataChunk.buffer());
                    dataChunk.seek(dataChunk.pos()+sz);
                    size_t blockAmount = sz / format.nBlockAlign;
                    blockCount += blockAmount;
                    for (size_t block = 0; block < blockAmount; block++)
                    {
                        for (size_t n = 0; n < format.nBlockAlign / (format.nChannels * 4u); n++)
                        {
                            for (size_t s = 0; s < format.nChannels; s++)
                            {
                                //reinterpret_cast<uint32_t *>(transformOut + block * format.nBlockAlign)[n * format.nChannels + s] =
                                //reinterpret_cast<uint32_t *>(transformIn + block * format.nBlockAlign)[s * format.nBlockAlign / (format.nChannels * 4) + n];
                                transformOut[(uint)((block*format.nBlockAlign)+(n*format.nChannels)+s)] =
                                        transformIn[(uint)((s * format.nBlockAlign) / (format.nChannels * 4) + n)];
                            }
                        }
                        outFile.write(transformOut,(size_t)format.nBlockAlign*blockAmount);
                        outFile.flush();
                    }

                }
            } else {
                outFile.write(dataChunk.data(),dataChunk.size());
                outFile.flush();
            }
        } else if (format.wFormatTag==0xFFFE) {
            format.wFormatTag = 0x1;
            QBuffer dataChunk;
            //iterating through chunks until we find the data one.
            while (!data.atEnd()) {
                ChunkHeader header;
                dataStr >> header;
                if (header.ChunkId==dataChunkId) {
                    dataChunk.write(data.data(),header.dwChunkSize);
                }
                    data.seek(data.pos()+header.dwChunkSize);

            }
            if (dataChunk.size()==0) {
                continue;
            }
            QDataStream fileStream(&outFile);
            //write the wav file
            //Riff chunk
            header.ChunkId = RIFFChunkId;
            header.dwChunkSize = sizeof(Fourcc) + sizeof(ChunkHeader) + sizeof(WaveFormatExtensible) + sizeof(ChunkHeader) + dataChunk.size();
            fileStream << header;
            outFile.flush();
            //fwrite(&header, sizeof(header), 1, outfile);
            //WAVE chunk
            fileStream << WAVEChunkId;
            outFile.flush();
            //fwrite(&fcc, sizeof(fcc), 1, outfile);
            //fmt chunk
            header.ChunkId = fmtChunkId;
            header.dwChunkSize = sizeof(WaveFormatExtensible);
            fileStream << header;
            fileStream << format;
            outFile.flush();
            //fwrite(&header, sizeof(header), 1, outfile);
            //fwrite(&format, sizeof(format), 1, outfile);
            //data chnuk
            header.ChunkId = dataChunkId;
            header.dwChunkSize = dataChunk.size();
            fileStream << header;
            outFile.flush();

            //just write the whole data chunk
            outFile.write(dataChunk.buffer(),dataChunk.size());
            outFile.flush();
        } else if (format.wFormatTag=0xFFFF) {
            //that is actually OGG vorbis file
            //header is not parsed since it's bogus
            //do extract data though
            char *fn = tmpnam(nullptr);
            FILE *outfile = fopen(fn, "wb");
            fwrite(data.buffer(), sizeof(data.buffer()[0]), data.size(), outfile);
            fclose(outfile);
            {
                Wwise_RIFF_Vorbis ww(fn);
                ofstream out(outFile.fileName().toStdString(), ios::binary);
                ww.generate_ogg(out);
            }
            #ifdef _WIN32
                _unlink(fn);
            #else 
                unlink(fn);
            #endif
            revorb(outFile.fileName().toStdString().c_str());
            return;
        }


        data.close();
        outFile.close();



        progress.setValue(progress.value()+1);
    }


}
