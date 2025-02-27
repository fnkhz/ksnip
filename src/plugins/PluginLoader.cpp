/*
 * Copyright (C) 2021 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "PluginLoader.h"

PluginLoader::PluginLoader(const QSharedPointer<ILogger> &logger) :
	mLogger(logger)
{

}

QObject* PluginLoader::load(const QString &path) const
{
	QPluginLoader pluginLoader(path);

	if (pluginLoader.isLoaded()) {
		mLogger->log(QString("Loaded plugin %1").arg(path));
	} else if (!pluginLoader.metaData().isEmpty()) {
		mLogger->log(pluginLoader.errorString());
	}

	return pluginLoader.instance();
}
