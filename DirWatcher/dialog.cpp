#include "dialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QCoreApplication>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "stringutil.h"

#include "cppparser.h"
#include "ocparser.h"

void Dialog::readFileList(const char *basePath)
{
    DIR *dir;
    struct dirent *ptr;

    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        return;
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
        {
            continue;
        }
        else if(ptr->d_type == 8)    ///file
        {
            string filePath = basePath;
            string fileName = ptr->d_name;
            filePath = filePath + "/" + fileName;
            SrcFileModel fileModel;
            fileModel.fileName = fileName;
            fileModel.filePath = filePath;
            fileModel.isParsed = false;
            fileList.push_back(fileModel);
        }
        else if(ptr->d_type == 10)    ///link file
        {
            string filePath = basePath;
            string fileName = ptr->d_name;
            filePath = filePath + "/" + fileName;
            SrcFileModel fileModel;
            fileModel.fileName = fileName;
            fileModel.filePath = filePath;
            fileModel.isParsed = false;
            fileList.push_back(fileModel);
        }
        else if(ptr->d_type == 4)    ///dir
        {
            string filePath = basePath;
            string fileName = ptr->d_name;
            filePath = filePath + "/" + fileName;
            readFileList(filePath.c_str());
        }
    }
    closedir(dir);
    return;
}

Dialog::Dialog(QString file_storage, 
               QString dir_storage, 
               QWidget *parent)
    : QDialog(parent)
{
    this->file_storage = file_storage;
    this->dir_storage = dir_storage;
    
    list = new QListWidget();
    choose = new QPushButton("请选择项目文件夹");
    start = new QPushButton("开始混淆");
    edit_line = new QLineEdit();
    
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    edit_line->setMinimumWidth(300);
    choose->setMinimumWidth(140);

    connect(choose, SIGNAL(clicked(bool)), SLOT(choose_path()));
    connect(start, SIGNAL(clicked(bool)), SLOT(start_choosing()));
    
    QHBoxLayout *input = new QHBoxLayout();
    input->addWidget(edit_line);
    input->addWidget(choose);
    
    QVBoxLayout *all = new QVBoxLayout();
    all->addLayout(input);
    all->addWidget(list);
    all->addWidget(start);
    
    setLayout(all);
    setWindowTitle("项目代码混淆工具");
    setWindowFlags(Qt::Window);
    setGeometry(400,400,1024,632);
}

void Dialog::choose_path()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Choose Directory"), ".");
    edit_line->setText(path);
}

void Dialog::add_next_path(QString path)
{
    list->insertItem(0, path);
}

void Dialog::start_choosing()
{
    start->setEnabled(false);

    list->setMouseTracking(false);

    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->setSelectionRectVisible(true);
    list->setSelectionBehavior(QAbstractItemView::SelectColumns);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setDragEnabled(false);

    qDebug() << "list count: " << list->count() << endl;
    while(list->count() != 0)
    {
        QListWidgetItem *item = list->takeItem(0);
        list->removeItemWidget(item);
        delete item;
    }

    vector<SrcFileModel>().swap(fileList);

    QString path = edit_line->text();

    QByteArray ba = path.toUtf8();
    char *pathStr = ba.data();
    readFileList(pathStr);

    StringUtil stringUtil;
    for(size_t i=0; i<fileList.size(); i++)
    {
        SrcFileModel file = fileList.at(i);

        if (file.isParsed || stringUtil.EndWith(file.filePath, "sqlite3.c"))
        {
            qDebug() << "file: " << file.filePath.c_str() << " is Parsed!" << endl;
            continue;
        }

        if(stringUtil.EndWith(file.fileName, ".h"))
        {
            file.headerFilePath = file.filePath;

            findCppFileWithFileModel(file);
            findMMFileWithFileModel(file);
            findMFileWithFileModel(file);
            if (file.cppFilePath.length() > 0)
            {
                QString headFileString = QString("正在分析:");
                headFileString = headFileString.append(file.headerFilePath.c_str());
                list->addItem(headFileString);

                QString cppFilePathString = QString("正在分析:");
                cppFilePathString = cppFilePathString.append(file.cppFilePath.c_str());
                list->addItem(cppFilePathString);

                CppParser cppParser;
                cppParser.parseCppFile(file);
            }

            if(file.mmFilePath.length() > 0)
            {
                QString headFileString = QString("正在分析:");
                headFileString = headFileString.append(file.headerFilePath.c_str());
                list->addItem(headFileString);

                QString mFilePathString = QString("正在分析:");
                mFilePathString = mFilePathString.append(file.mmFilePath.c_str());
                list->addItem(mFilePathString);

                CppParser cppParser;
                cppParser.parseCppFile(file);
            }

            if(file.mFilePath.length() > 0)
            {
                QString headFileString = QString("正在分析:");
                headFileString = headFileString.append(file.headerFilePath.c_str());
                list->addItem(headFileString);

                QString mFilePathString = QString("正在分析:");
                mFilePathString = mFilePathString.append(file.mFilePath.c_str());
                list->addItem(mFilePathString);

                OCParser ocParser;
                ocParser.parseOCFile(file);
            }
        }
        else if(stringUtil.EndWith(file.fileName, ".cpp") || stringUtil.EndWith(file.fileName, ".cxx") || stringUtil.EndWith(file.fileName, ".cc"))
        {
            file.cppFileName = file.fileName;
            file.cppFilePath = file.filePath;

            if(findHeaderFileWithFileModel(file))
            {
                QString headerFilePathString = QString("正在分析:");
                headerFilePathString = headerFilePathString.append(file.headerFilePath.c_str());
                list->addItem(headerFilePathString);
            }
            QString cppFilePathString = QString("正在分析:");
            cppFilePathString = cppFilePathString.append(file.cppFilePath.c_str());
            list->addItem(cppFilePathString);

            CppParser cppParser;
            cppParser.parseCppFile(file);
        }
        else if(stringUtil.EndWith(file.fileName, ".c"))
        {
            findHeaderFileWithFileModel(file);

            file.cFileName = file.fileName;
            file.cFilePath = file.filePath;

            QString parseInfoString = QString("正在分析:");
            parseInfoString = parseInfoString.append(file.filePath.c_str());
            list->addItem(parseInfoString);

            CppParser cppParser;
            cppParser.parseCppFile(file);
        }
        else if(stringUtil.EndWith(file.fileName, ".m"))
        {
            file.mFileName = file.fileName;
            file.mFilePath = file.filePath;

            if(findHeaderFileWithFileModel(file))
            {
                QString headerFilePathString = QString("正在分析:");
                headerFilePathString = headerFilePathString.append(file.headerFilePath.c_str());
                list->addItem(headerFilePathString);
            }

            QString parseInfoString = QString("正在分析:");
            parseInfoString = parseInfoString.append(file.filePath.c_str());
            list->addItem(parseInfoString);

            OCParser ocParser;
            ocParser.parseOCFile(file);
        }
        else if(stringUtil.EndWith(file.fileName, ".mm"))
        {
            findHeaderFileWithFileModel(file);

            file.mmFileName = file.fileName;
            file.mmFilePath = file.filePath;

            QString parseInfoString = QString("正在分析:");
            parseInfoString = parseInfoString.append(file.filePath.c_str());
            list->addItem(parseInfoString);

            OCParser ocParser;
            ocParser.parseOCFile(file);

            CppParser cppParser;
            cppParser.parseCppFile(file);
        }
        else
        {
            qDebug() << "跳过其他文件: " << file.filePath.c_str() << endl;
        }

        list->update();
        list->repaint();
        list->scrollToBottom();
        QCoreApplication::processEvents();
    }

    list->setEditTriggers(QAbstractItemView::AllEditTriggers);
    list->setSelectionBehavior(QAbstractItemView::SelectItems);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setDragEnabled(true);

    start->setEnabled(true);
}

bool Dialog::findCppFileWithFileModel(SrcFileModel &fileModel)
{
    QFile srcFile(fileModel.filePath.c_str());
    QFileInfo fileInfo = QFileInfo(srcFile);

    string cppFileName = fileInfo.baseName().append(".cpp").toStdString();
    string ccFileName = fileInfo.baseName().append(".cc").toStdString();
    string cxxFileName = fileInfo.baseName().append(".cxx").toStdString();

    StringUtil stringUtil;
    for(size_t i=0; i<fileList.size(); i++)
    {
        SrcFileModel file = fileList.at(i);
        if(stringUtil.EndWith(file.filePath, cppFileName) || stringUtil.EndWith(file.filePath, ccFileName) || stringUtil.EndWith(file.filePath, cxxFileName))
        {
            QFile qfile(file.filePath.c_str());
            QFileInfo fi = QFileInfo(qfile);
            QString fileBaseName = fi.fileName();
            QString cppFilePath = fi.absolutePath();
            cppFilePath = cppFilePath.append("/").append(fileBaseName);
            fileModel.cppFilePath = cppFilePath.toStdString();
            fileModel.cppFileName = fileBaseName.toStdString();

            file.isParsed = true;
            SrcFileModel tempModel = file;
            tempModel.isParsed = true;
            fileList[i] = tempModel;
            return true;
        }
    }

    return false;
}

//Objective C
bool Dialog::findMFileWithFileModel(SrcFileModel &fileModel)
{
    QFile srcFile(fileModel.filePath.c_str());
    QFileInfo fileInfo = QFileInfo(srcFile);

    string ocFileName = fileInfo.baseName().append(".m").toStdString();

    StringUtil stringUtil;
    for(size_t i=0; i<fileList.size(); i++)
    {
        SrcFileModel file = fileList.at(i);
        if(stringUtil.EndWith(file.filePath, ocFileName))
        {
            QFile qfile(file.filePath.c_str());
            QFileInfo fi = QFileInfo(qfile);
            QString fileBaseName = fi.baseName();
            fileBaseName = fileBaseName.append(".m");
            QString mFilePath = fi.absolutePath();
            mFilePath = mFilePath.append("/").append(fileBaseName);
            fileModel.mFilePath = mFilePath.toStdString();
            fileModel.mFileName = fileBaseName.toStdString();
            qDebug() << "find m file: " << fileModel.mFilePath.c_str() << endl;

            file.isParsed = true;
            SrcFileModel tempModel = file;
            tempModel.isParsed = true;
            fileList[i] = tempModel;
            return true;
        }
    }

    return false;
}

//Objective C++
bool Dialog::findMMFileWithFileModel(SrcFileModel &fileModel)
{
    QFile srcFile(fileModel.filePath.c_str());
    QFileInfo fileInfo = QFileInfo(srcFile);

    string ocFileName = fileInfo.baseName().append(".m").toStdString();

    StringUtil stringUtil;
    for(size_t i=0; i<fileList.size(); i++)
    {
        SrcFileModel file = fileList.at(i);
        if(stringUtil.EndWith(file.filePath, ocFileName))
        {
            QFile qfile(file.filePath.c_str());
            QFileInfo fi = QFileInfo(qfile);
            QString fileBaseName = fi.baseName();
            fileBaseName = fileBaseName.append(".mm");
            QString mmFilePath = fi.absolutePath();
            mmFilePath = mmFilePath.append("/").append(fileBaseName);
            fileModel.mmFilePath = mmFilePath.toStdString();
            fileModel.mmFileName = fileBaseName.toStdString();
            qDebug() << "find mm file: " << fileModel.mmFilePath.c_str() << endl;

            file.isParsed = true;
            SrcFileModel tempModel = file;
            tempModel.isParsed = true;
            fileList[i] = tempModel;
            return true;
        }
    }

    return false;
}

bool Dialog::findHeaderFileWithFileModel(SrcFileModel &fileModel)
{
    QFile qSrcfile(fileModel.filePath.c_str());
    QFileInfo fileInfo = QFileInfo(qSrcfile);

    string headerFileName = fileInfo.baseName().append(".h").toStdString();
    StringUtil stringUtil;
    for(size_t i=0; i<fileList.size(); i++)
    {
        SrcFileModel file = fileList.at(i);
        if(stringUtil.EndWith(file.filePath, headerFileName))
        {
            QFile qfile(file.filePath.c_str());
            QFileInfo fi = QFileInfo(qfile);
            QString fileBaseName = fi.baseName();
            fileBaseName = fileBaseName.append(".h");
            QString headerFilePath = fi.absolutePath();
            headerFilePath = headerFilePath.append("/").append(fileBaseName);
            fileModel.headerFilePath = headerFilePath.toStdString();
            fileModel.headerFileName = fileBaseName.toStdString();
            qDebug() << "find header file: " << fileModel.headerFilePath.c_str() << endl;

            SrcFileModel tempModel = file;
            tempModel.isParsed = true;
            fileList[i] = tempModel;
            return true;
        }
    }

    return false;
}

Dialog::~Dialog()
{
    
}