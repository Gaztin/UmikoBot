#include "LevelModule.h"
#include "UmikoBot.h"

LevelModule::LevelModule()
	: Module("levels", true)
{
	m_timer.setInterval(/*300 */ 1000);
	QObject::connect(&m_timer, &QTimer::timeout, 
		[this]()
	{
		for (auto it = m_exp.begin(); it != m_exp.end(); it++)
		{
			for (GuildLevelData& data : it.value())
			{
				if (data.messageCount > 0) 
				{
					data.messageCount = 0;
					data.exp += qrand() % 31 + 15;
				}
			}
		}
	});
	m_timer.start();

	QTime now = QTime::currentTime();
	qsrand(now.msec());

	RegisterCommand(Commands::LEVEL_MODULE_TOP, "top",
		[this](Discord::Client& client, const Discord::Message& message, const Discord::Channel& channel, const Discord::GuildMember& member)
	{
		QStringList args = message.content().split(' ');
		if (args.size() == 2) {
			qSort(m_exp[channel.guildId()].begin(), m_exp[channel.guildId()].end(),
				[](const LevelModule::GuildLevelData& v1, const LevelModule::GuildLevelData& v2) -> bool
			{
				return v1.exp > v2.exp;
			});

			Discord::Embed embed;
			embed.setColor(qrand() % 16777216);
			embed.setTitle("Top " + args.back());

			QString desc = "";
			for (size_t i = 0; i < args.back().toUInt(); i++) 
			{
				if (i >= m_exp[channel.guildId()].size())
				{
					embed.setTitle("Top " + QString::number(i));
					break;
				}

				LevelModule::GuildLevelData& curr = m_exp[channel.guildId()][i];
				desc += QString::number(i + 1) + ". ";
				desc += reinterpret_cast<UmikoBot*>(&client)->GetNick(channel.guildId(), m_exp[channel.guildId()][i].user);
				desc += " - " + QString::number(curr.exp) + "\n";
			}

			embed.setDescription(desc);

			client.createMessage(message.channelId(), embed);
		}
	});
}

void LevelModule::OnSave(QJsonDocument& doc) const
{
	QJsonObject json;
	for (auto it = m_exp.begin(); it != m_exp.end(); it++) {
		QJsonObject level;

		for (const GuildLevelData& user : it.value()) {
			level[QString::number(user.user)] = user.exp;
		}
		json[QString::number(it.key())] = level;
	}

	doc.setObject(json);
}

void LevelModule::OnLoad(const QJsonDocument& doc)
{
	QJsonObject json = doc.object();

	QStringList guildIds = json.keys();

	for (const QString& guild : guildIds)
	{
		snowflake_t guildId = guild.toULongLong();

		QJsonObject levels = json[guild].toObject();

		QStringList userids = levels.keys();

		for (const QString& user : userids)
			m_exp[guildId].append({ user.toULongLong(), levels[user].toInt(), 0 });
	}
}

void LevelModule::StatusCommand(QString& result, snowflake_t guild, snowflake_t user)
{
	GuildSetting s = GuildSettings::GetGuildSetting(guild);

	if (s.ranks.size() > 0) {
		result += "Rank: ...\n";
	}
	unsigned int xp = GetData(guild, user).exp;
	result += "Total exp: " + QString::number(xp) + "\n";
	
	unsigned int xpRequirement = LEVELMODULE_EXP_REQUIREMENT;
	unsigned int level = 1;
	while (xp > xpRequirement && level < LEVELMODULE_MAXIMUM_LEVEL) {
		level++;
		xp -= xpRequirement;
		xpRequirement *= 1.5;
	}

	if (level >= LEVELMODULE_MAXIMUM_LEVEL)
	{
		xp = 0;
		level = LEVELMODULE_MAXIMUM_LEVEL;
		xpRequirement = 0;
	}

	result += "Level: " + QString::number(level) + "\n";
	if(xp == 0 && xpRequirement == 0)
		result += QString("Exp needed for rankup: Maximum Level\n");
	else
		result += QString("Exp needed for rankup: %1/%2\n").arg(QString::number(xp), QString::number(xpRequirement));
	result += "\n";
}

void LevelModule::OnMessage(Discord::Client& client, const Discord::Message& message) 
{
	Module::OnMessage(client, message);

	client.getChannel(message.channelId()).then(
		[this, message](const Discord::Channel& channel) 
	{
		if (message.author().bot())
			return;

		for (GuildLevelData& data : m_exp[channel.guildId()]) {
			if (data.user == message.author().id()) {
				data.messageCount++;
				return;
			}
		}

		m_exp[channel.guildId()].append({ message.author().id(), 0, 1 });
	});
}

LevelModule::GuildLevelData LevelModule::GetData(snowflake_t guild, snowflake_t user)
{
	for (GuildLevelData data : m_exp[guild])
	{
		if (data.user == user) {
			return data;
		}
	}
	return { user, 0,0 };
}
