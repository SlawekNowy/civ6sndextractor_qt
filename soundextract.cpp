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
        xml->Parent();

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
            addedWaves.append(QString::fromStdString(sound.name));
        }
        if (addedWaves.size()==0) {
            ui->soundWavesOpened->addItems(addedWaves);
            savedSounds.reserve(sounds.size()); //reserve the space for x sounds
            savedSounds.insert(savedSounds.end(),sounds.begin(),sounds.end()); // insert those at the end of the vector
            sounds.clear();
            sounds.shrink_to_fit();
            //just making these a bit faster.
        }
    }
}

void MainWindow::on_extractButton_clicked()
{

}
