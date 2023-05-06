/* SleepLib Resvent Loader Implementation
 *
 * Copyright (c) 2019-2023 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// Please only INCREMENT the resvent_data_version in resvent_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

#include <QString>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QDebug>
#include <QStringList>
#include <cmath>
#include <array>
#include <unordered_map>

#include "resvent_loader.h"

#ifdef DEBUG_EFFICIENCY
#include <QElapsedTimer>  // only available in 4.8
#endif

// Files WXX_XX contain flow rate and pressure and PXX_XX contain Pressure, IPAP, EPAP, Leak, Vt, MV, RR, Ti, IE, Spo2, PR
// Both files contain a little header of size 0x24 bytes. In offset 0x12 contain the total number of different records in
// the different files of the same type. And later contain the previous describe quantity of description header of size 0x20
// containing the details for every type of record (e.g. sample chunk size).

ResventLoader::ResventLoader()
{
    const QString RESVENT_ICON = ":/icons/resvent.png";

    QString s = newInfo().series;
    m_pixmap_paths[s] = RESVENT_ICON;
    m_pixmaps[s] = QPixmap(RESVENT_ICON);

    m_type = MT_CPAP;
}
ResventLoader::~ResventLoader()
{
}

const QString kResventTherapyFolder = "THERAPY";
const QString kResventConfigFolder = "CONFIG";
const QString kResventRecordFolder = "RECORD";
const QString kResventSysConfigFilename = "SYSCFG";
constexpr qint64 kDateTimeOffset = 7 * 60 * 60 * 1000;
constexpr int kMainHeaderSize = 0x24;
constexpr int kDescriptionHeaderSize = 0x20;
constexpr int kChunkDurationInSecOffset = 0x10;
constexpr int kDescriptionCountOffset = 0x12;
constexpr int kDescriptionSamplesByChunk = 0x1e;

bool ResventLoader::Detect(const QString & givenpath)
{
    QDir dir(givenpath);

    if (!dir.exists()) {
        return false;
    }

    if (!dir.exists(kResventTherapyFolder)) {
        return false;
    }

    dir.cd(kResventTherapyFolder);
    if (!dir.exists(kResventConfigFolder)) {
        return false;
    }

    if (!dir.exists(kResventRecordFolder)) {
        return false;
    }

    return true;
}


MachineInfo ResventLoader::PeekInfo(const QString & path)
{
    if (!Detect(path)) {
        return MachineInfo();
    }

    MachineInfo info = newInfo();

    const auto sys_config_path = path + QDir::separator() + kResventTherapyFolder + QDir::separator() + kResventConfigFolder + QDir::separator() + kResventSysConfigFilename;
    if (!QFile::exists(sys_config_path)) {
        qDebug() << "Resvent Data card has no" << kResventSysConfigFilename << "file in " << sys_config_path;
        return MachineInfo();
    }
    QFile f(sys_config_path);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    f.seek(4);
    while (!f.atEnd()) {
        QString line = f.readLine().trimmed();

        const auto elems = line.split("=");
        assert(elems.size() == 2);

        if (elems[0] == "models") {
            info.model = elems[1];
        }
        else if (elems[0] == "sn") {
            info.serial = elems[1];
        }
        else if (elems[0] == "num") {
            info.version = elems[1].toInt();
        }
        else if (elems[0] == "num") {
            info.type = MachineType::MT_CPAP;
        }
    }

    if (info.model.contains("Point", Qt::CaseInsensitive)) {
        info.brand = "Hoffrichter";
    } else {
        info.brand = "Resvent";
    }

    return info;
}

std::vector<QDate> GetSessionsDate(const QString& dirpath) {
    std::vector<QDate> sessions_date;

    const auto records_path = dirpath + QDir::separator() + kResventTherapyFolder + QDir::separator() + kResventRecordFolder;
    QDir records_folder(records_path);
    const auto year_month_folders = records_folder.entryList(QStringList(), QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name);
    std::for_each(year_month_folders.cbegin(), year_month_folders.cend(), [&](const QString& year_month_folder_name){
        if (year_month_folder_name.length() != 6) {
            return;
        }

        const int year = std::stoi(year_month_folder_name.left(4).toStdString());
        const int month = std::stoi(year_month_folder_name.right(2).toStdString());

        const auto year_month_folder_path = records_path + QDir::separator() + year_month_folder_name;
        QDir year_month_folder(year_month_folder_path);
        const auto session_folders = year_month_folder.entryList(QStringList(), QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name);
        std::for_each(session_folders.cbegin(), session_folders.cend(), [&](const QString& day_folder){
            const auto day = std::stoi(day_folder.toStdString());

            sessions_date.push_back(QDate(year, month, day));
        });
    });
    return sessions_date;
}

enum class EventType {
    UsageSec= 1,
    UnixStart = 2,
    ObstructiveApnea = 17,
    CentralApnea = 18,
    Hypopnea = 19,
    FlowLimitation = 20,
    RERA = 21,
    PeriodicBreathing = 22,
    Snore = 23
};

struct EventData {
    EventType type;
    QDateTime date_time;
    int duration;
};

struct UsageData {
    QString number{};
    QDateTime start_time{};
    QDateTime end_time{};
    qint32 countAHI = 0;
    qint32 countOAI = 0;
    qint32 countCAI = 0;
    qint32 countAI = 0;
    qint32 countHI = 0;
    qint32 countRERA = 0;
    qint32 countSNI = 0;
    qint32 countBreath = 0;
};

void UpdateEvents(EventType event_type, const std::unordered_map<EventType, std::vector<EventData>>& events, Session* session) {
    static std::unordered_map<EventType, unsigned int> mapping {{EventType::ObstructiveApnea, CPAP_Obstructive},
                                                               {EventType::CentralApnea, CPAP_Apnea},
                                                               {EventType::Hypopnea, CPAP_Hypopnea},
                                                               {EventType::FlowLimitation, CPAP_FlowLimit},
                                                               {EventType::RERA, CPAP_RERA},
                                                               {EventType::PeriodicBreathing, CPAP_PB},
                                                               {EventType::Snore, CPAP_Snore}};
    const auto it_events = events.find(event_type);
    const auto it_mapping = mapping.find(event_type);
    if (it_events == events.cend() || it_mapping == mapping.cend()) {
        return;
    }

    EventList* event_list  = session->AddEventList(it_mapping->second, EVL_Event);
    std::for_each(it_events->second.cbegin(), it_events->second.cend(), [&](const EventData& event_data){
        event_list->AddEvent(event_data.date_time.toMSecsSinceEpoch() + kDateTimeOffset, event_data.duration);
    });
}

QString GetSessionFolder(const QString& dirpath, const QDate& session_date) {
    const auto year_month_folder = QString::number(session_date.year()) + (session_date.month() > 10 ? "" : "0") + QString::number(session_date.month());
    const auto day_folder = (session_date.day() > 10 ? "" : "0") + QString::number(session_date.day());
    const auto session_folder_path = dirpath + QDir::separator() + kResventTherapyFolder + QDir::separator() + kResventRecordFolder + QDir::separator() + year_month_folder + QDir::separator() + day_folder;
    return session_folder_path;
}

void LoadEvents(const QString& session_folder_path, Session* session, const UsageData& usage) {
    const auto event_file_path = session_folder_path + QDir::separator() + "EV" + usage.number;

    std::unordered_map<EventType, std::vector<EventData>> events;
    QFile f(event_file_path);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    f.seek(4);
    while (!f.atEnd()) {
        QString line = f.readLine().trimmed();

        const auto elems = line.split(",", Qt::SkipEmptyParts);
        if (elems.size() != 4) {
            continue;
        }

        const auto event_type_elems = elems.at(0).split("=");
        const auto date_time_elems = elems.at(1).split("=");
        const auto duration_elems = elems.at(2).split("=");

        assert(event_type_elems.size() == 2);
        assert(date_time_elems.size() == 2);
        const auto event_type = static_cast<EventType>(std::stoi(event_type_elems[1].toStdString()));
        const auto date_time = QDateTime::fromTime_t(std::stoi(date_time_elems[1].toStdString()));
        const auto duration = std::stoi(duration_elems[1].toStdString());

        events[event_type].push_back(EventData{event_type, date_time, duration});
    }

    static std::vector<EventType> mapping {EventType::ObstructiveApnea,
                                          EventType::CentralApnea,
                                          EventType::Hypopnea,
                                          EventType::FlowLimitation,
                                          EventType::RERA,
                                          EventType::PeriodicBreathing,
                                          EventType::Snore};

    std::for_each(mapping.cbegin(), mapping.cend(), [&](EventType event_type){
        UpdateEvents(event_type, events, session);
    });
}

template <typename T>
T read_from_file(QFile& f) {
    T data{};
    f.read(reinterpret_cast<char*>(&data), sizeof(T));
    return data;
}

struct WaveFileData {
    unsigned int wave_event_id;
    QString file_base_name;
    unsigned int sample_rate_offset;
    unsigned int start_offset;
};

EventList* GetEventList(const QString& name, Session* session, float sample_rate = 0.0) {
    if (name == "Press") {
        return session->AddEventList(CPAP_Pressure, EVL_Event);
    }
    else if (name == "IPAP") {
        return session->AddEventList(CPAP_IPAP, EVL_Event);
    }
    else if (name == "EPAP") {
        return session->AddEventList(CPAP_EPAP, EVL_Event);
    }
    else if (name == "Leak") {
        return session->AddEventList(CPAP_Leak, EVL_Event);
    }
    else if (name == "Vt") {
        return session->AddEventList(CPAP_TidalVolume, EVL_Event);
    }
    else if (name == "MV") {
        return session->AddEventList(CPAP_MinuteVent, EVL_Event);
    }
    else if (name == "RR") {
        return session->AddEventList(CPAP_RespRate, EVL_Event);
    }
    else if (name == "Ti") {
        return session->AddEventList(CPAP_Ti, EVL_Event);
    }
    else if (name == "I:E") {
        return session->AddEventList(CPAP_IE, EVL_Event);
    }
    else if (name == "SpO2" || name == "PR") {
        // Not present
        return nullptr;
    }
    else if (name == "Pressure") {
        return session->AddEventList(CPAP_MaskPressure, EVL_Waveform, 0.01, 0.0, 0.0, 0.0, 1000.0 / sample_rate);
    }
    else if (name == "Flow") {
        return session->AddEventList(CPAP_FlowRate, EVL_Waveform, 0.01, 0.0, 0.0, 0.0, 1000.0 / sample_rate);
    }
    else {
        // Not supported
        assert(false);
        return nullptr;
    }
}

struct ChunkData {
    EventList* event_list;
    uint16_t samples_by_chunk;
    qint64 start_time;
    int total_samples_by_chunk;
    float sample_rate;
};

QString ReadDescriptionName(QFile& f) {
    constexpr int kNameSize = 9;
    std::array<char, kNameSize> name;
    const auto readed = f.read(name.data(), kNameSize - 1);
    assert(readed == kNameSize - 1);

    return QString(name.data());
}

void ReadWaveFormsHeaders(QFile& f, std::vector<ChunkData>& wave_forms, Session* session, const UsageData& usage) {
    f.seek(kChunkDurationInSecOffset);
    const auto chunk_duration_in_sec = read_from_file<uint16_t>(f);
    f.seek(kDescriptionCountOffset);
    const auto description_count = read_from_file<uint16_t>(f);
    wave_forms.resize(description_count);

    for (unsigned int i = 0; i < description_count; i++) {
        const auto description_header_offset = kMainHeaderSize + i * kDescriptionHeaderSize;
        f.seek(description_header_offset);
        const auto name = ReadDescriptionName(f);
        f.seek(description_header_offset + kDescriptionSamplesByChunk);
        const auto samples_by_chunk = read_from_file<uint16_t>(f);

        wave_forms[i].sample_rate = 1.0 * samples_by_chunk / chunk_duration_in_sec;
        wave_forms[i].event_list = GetEventList(name, session, wave_forms[i].sample_rate);
        wave_forms[i].samples_by_chunk = samples_by_chunk;
        wave_forms[i].start_time = usage.start_time.toMSecsSinceEpoch();
    }
}

void LoadOtherWaveForms(const QString& session_folder_path, Session* session, const UsageData& usage) {
    QDir session_folder(session_folder_path);

    const auto wave_files = session_folder.entryList(QStringList() << "P" + usage.number + "_*", QDir::Files, QDir::Name);

    std::vector<ChunkData> wave_forms;
    bool initialized = false;
    std::for_each(wave_files.cbegin(), wave_files.cend(), [&](const QString& wave_file){
        // W01_ file
        QFile f(session_folder_path + QDir::separator() + wave_file);
        f.open(QIODevice::ReadOnly);

        if (!initialized) {
            ReadWaveFormsHeaders(f, wave_forms, session, usage);
            initialized = true;
        }
        f.seek(kMainHeaderSize + wave_forms.size() * kDescriptionHeaderSize);

        std::vector<qint16> chunk(std::max_element(wave_forms.cbegin(), wave_forms.cend(), [](const ChunkData& lhs, const ChunkData& rhs){
                                      return lhs.samples_by_chunk < rhs.samples_by_chunk;
                                  })->samples_by_chunk);
        while (!f.atEnd()) {
            for (unsigned int i = 0; i < wave_forms.size(); i++) {
                const auto& wave_form = wave_forms[i].event_list;
                const auto samples_by_chunk_actual = wave_forms[i].samples_by_chunk;
                auto& start_time_current = wave_forms[i].start_time;
                auto& total_samples_by_chunk = wave_forms[i].total_samples_by_chunk;
                const auto sample_rate = wave_forms[i].sample_rate;

                const auto readed = f.read(reinterpret_cast<char*>(chunk.data()), chunk.size() * sizeof(qint16));
                if (wave_form) {
                    const auto readed_elements = readed / sizeof(qint16);
                    if (readed_elements != samples_by_chunk_actual) {
                        std::fill(std::begin(chunk) + readed_elements, std::end(chunk), 0);
                    }

                    int offset = 0;
                    std::for_each(chunk.cbegin(), chunk.cend(), [&](const qint16& value){
                        wave_form->AddEvent(start_time_current + offset + kDateTimeOffset, value);
                        offset += 1000.0 / sample_rate;
                    });
                }

                start_time_current += samples_by_chunk_actual * 1000.0 / sample_rate;
                total_samples_by_chunk += samples_by_chunk_actual;
            }
        }
    });

    std::vector<qint16> chunk;
    for (unsigned int i = 0; i < wave_forms.size(); i++) {
        const auto& wave_form = wave_forms[i];
        const auto expected_samples = usage.start_time.msecsTo(usage.end_time) / 1000.0 * wave_form.sample_rate;
        if (wave_form.total_samples_by_chunk < expected_samples) {
            chunk.resize(expected_samples - wave_form.total_samples_by_chunk);
            if (wave_form.event_list) {
                int offset = 0;
                std::for_each(chunk.cbegin(), chunk.cend(), [&](const qint16& value){
                    wave_form.event_list->AddEvent(wave_form.start_time + offset + kDateTimeOffset, value);
                    offset += 1000.0 / wave_form.sample_rate;
                });
            }
        }
    }
}

void LoadWaveForms(const QString& session_folder_path, Session* session, const UsageData& usage) {
    QDir session_folder(session_folder_path);

    const auto wave_files = session_folder.entryList(QStringList() << "W" + usage.number + "_*", QDir::Files, QDir::Name);

    std::vector<ChunkData> wave_forms;
    bool initialized = false;

    std::for_each(wave_files.cbegin(), wave_files.cend(), [&](const QString& wave_file){
        // W01_ file
        QFile f(session_folder_path + QDir::separator() + wave_file);
        f.open(QIODevice::ReadOnly);

        if (!initialized) {
            ReadWaveFormsHeaders(f, wave_forms, session, usage);
            initialized = true;
        }
        f.seek(kMainHeaderSize + wave_forms.size() * kDescriptionHeaderSize);

        std::vector<qint16> chunk(std::max_element(wave_forms.cbegin(), wave_forms.cend(), [](const ChunkData& lhs, const ChunkData& rhs){
                                      return lhs.samples_by_chunk < rhs.samples_by_chunk;
                                  })->samples_by_chunk);
        while (!f.atEnd()) {
            for (unsigned int i = 0; i < wave_forms.size(); i++) {
                const auto& wave_form = wave_forms[i].event_list;
                const auto samples_by_chunk_actual = wave_forms[i].samples_by_chunk;
                auto& start_time_current = wave_forms[i].start_time;
                auto& total_samples_by_chunk = wave_forms[i].total_samples_by_chunk;
                const auto sample_rate = wave_forms[i].sample_rate;

                const auto duration = samples_by_chunk_actual * 1000.0 / sample_rate;
                const auto readed = f.read(reinterpret_cast<char*>(chunk.data()), chunk.size() * sizeof(qint16));
                if (wave_form) {
                    const auto readed_elements = readed / sizeof(qint16);
                    if (readed_elements != samples_by_chunk_actual) {
                        std::fill(std::begin(chunk) + readed_elements, std::end(chunk), 0);
                    }
                    wave_form->AddWaveform(start_time_current + kDateTimeOffset, chunk.data(), samples_by_chunk_actual, duration);
                }

                start_time_current += duration;
                total_samples_by_chunk += samples_by_chunk_actual;
            }
        }
    });

    std::vector<qint16> chunk;
    for (unsigned int i = 0; i < wave_forms.size(); i++) {
        const auto& wave_form = wave_forms[i];
        const auto expected_samples = usage.start_time.msecsTo(usage.end_time) / 1000.0 * wave_form.sample_rate;
        if (wave_form.total_samples_by_chunk < expected_samples) {
            chunk.resize(expected_samples - wave_form.total_samples_by_chunk);
            if (wave_form.event_list) {
                const auto duration = chunk.size() * 1000.0 / wave_form.sample_rate;
                wave_form.event_list->AddWaveform(wave_form.start_time + kDateTimeOffset, chunk.data(), chunk.size(), duration);
            }
        }
    }
}

void LoadStats(const UsageData& /*usage_data*/, Session* session) {
    //    session->settings[CPAP_AHI] = usage_data.countAHI;
    //    session->setCount(CPAP_AI, usage_data.countAI);
    //    session->setCount(CPAP_CAI, usage_data.countCAI);
    //    session->setCount(CPAP_HI, usage_data.countHI);
    //    session->setCount(CPAP_Obstructive, usage_data.countOAI);
    //    session->settings[CPAP_RERA] = usage_data.countRERA;
    //    session->settings[CPAP_Snore] = usage_data.countSNI;
    session->settings[CPAP_Mode] = MODE_APAP;
}

UsageData ReadUsage(const QString& session_folder_path, const QString& usage_number) {
    UsageData usage_data;
    usage_data.number = usage_number;

    const auto session_stat_path = session_folder_path + QDir::separator() + "STAT" + usage_number;
    if (!QFile::exists(session_stat_path)) {
        qDebug() << "Resvent Data card has no " << session_stat_path;
        return usage_data;
    }
    QFile f(session_stat_path);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    f.seek(4);
    while (!f.atEnd()) {
        QString line = f.readLine().trimmed();

        const auto elems = line.split("=");
        assert(elems.size() == 2);

        if (elems[0] == "secStart") {
            usage_data.start_time = QDateTime::fromTime_t(std::stoi(elems[1].toStdString()));
        }
        else if (elems[0] == "secUsed") {
            usage_data.end_time = QDateTime::fromTime_t(usage_data.start_time.toTime_t() + std::stoi(elems[1].toStdString()));
        }
        else if (elems[0] == "cntAHI") {
            usage_data.countAHI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntOAI") {
            usage_data.countOAI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntCAI") {
            usage_data.countCAI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntAI") {
            usage_data.countAI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntHI") {
            usage_data.countHI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntRERA") {
            usage_data.countRERA = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntSNI") {
            usage_data.countSNI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntBreath") {
            usage_data.countBreath = std::stoi(elems[1].toStdString());
        }
    }

    return usage_data;
}

std::vector<UsageData> GetDifferentUsage(const QString& session_folder_path) {
    QDir session_folder(session_folder_path);

    const auto stat_files = session_folder.entryList(QStringList() << "STAT*", QDir::Files, QDir::Name);
    std::vector<UsageData> usage_data;
    std::for_each(stat_files.cbegin(), stat_files.cend(), [&](const QString& stat_file){
        if (stat_file.size() != 6) {
            return;
        }

        auto usageData = ReadUsage(session_folder_path, stat_file.right(2));

        usage_data.push_back(usageData);
    });
    return usage_data;
}

int LoadSession(const QString& dirpath, const QDate& session_date, Machine* machine) {
    const auto session_folder_path = GetSessionFolder(dirpath, session_date);

    const auto different_usage = GetDifferentUsage(session_folder_path);

    return std::accumulate(different_usage.cbegin(), different_usage.cend(), 0, [&](int base, const UsageData& usage){
        if (machine->SessionExists(usage.start_time.toMSecsSinceEpoch() + kDateTimeOffset)) {
            return base;
        }
        Session* session = new Session(machine, usage.start_time.toMSecsSinceEpoch() + kDateTimeOffset);
        session->SetChanged(true);
        session->really_set_first(usage.start_time.toMSecsSinceEpoch() + kDateTimeOffset);
        session->really_set_last(usage.end_time.toMSecsSinceEpoch() + kDateTimeOffset);

        LoadStats(usage, session);
        LoadWaveForms(session_folder_path, session, usage);
        LoadOtherWaveForms(session_folder_path, session, usage);
        LoadEvents(session_folder_path, session, usage);

        session->UpdateSummaries();
        session->Store(machine->getDataPath());
        machine->AddSession(session);
        return base + 1;
    });
}


///////////////////////////////////////////////////////////////////////////////////////////
// Sorted EDF files that need processing into date records according to ResMed noon split
///////////////////////////////////////////////////////////////////////////////////////////
int ResventLoader::Open(const QString & dirpath)
{
    const auto machine_info = PeekInfo(dirpath);

    // Abort if no serial number
    if (machine_info.serial.isEmpty()) {
        qDebug() << "Resvent Data card has no valid serial number in " << kResventSysConfigFilename;
        return -1;
    }

    const auto sessions_date = GetSessionsDate(dirpath);

    Machine *machine = p_profile->CreateMachine(machine_info);

    int new_sessions = 0;
    std::for_each(sessions_date.cbegin(), sessions_date.cend(), [&](const QDate& session_date){
        new_sessions += LoadSession(dirpath, session_date, machine);
    });

    machine->Save();

    return new_sessions;
}

void ResventLoader::initChannels()
{
}

ChannelID ResventLoader::PresReliefMode() { return 0; }
ChannelID ResventLoader::PresReliefLevel() { return 0; }
ChannelID ResventLoader::CPAPModeChannel() { return 0; }

bool resvent_initialized = false;
void ResventLoader::Register()
{
    if (resvent_initialized) { return; }

    qDebug() << "Registering ResventLoader";
    RegisterLoader(new ResventLoader());

    resvent_initialized = true;
}
