/*
 *  Copyright (C) 2016 Damir Porobic <https://github.com/damirporobic>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "ImgurWrapper.h"

ImgurWrapper::ImgurWrapper(const QString &imgurUrl, QObject *parent) :
	mBaseImgutUrl(imgurUrl),
	QObject(parent),
    mAccessManager(new QNetworkAccessManager(this))
{
    connect(mAccessManager, &QNetworkAccessManager::finished, this, &ImgurWrapper::handleReply);

    // Client ID that will only be used for anonymous upload
    mClientId = "16d41e28a3ba71e";
}

/*
 * This function starts the upload, depending if an access token was provided
 * this will be either an account upload on an anonymous upload. If the upload
 * was successful the upload Fished signal will be emitted which holds the url
 * to the image.
 */
void ImgurWrapper::startUpload(const QImage& image, const QString &title, const QString &description, const QByteArray& accessToken) const
{
    // Convert the image into a byteArray
    QByteArray imageByteArray;
    QBuffer buffer(&imageByteArray);
    image.save(&buffer, "PNG");

    // Create the network request for posting the image
    QUrl url(mBaseImgutUrl + QLatin1String("/3/upload.xml"));
    QUrlQuery urlQuery;

    // Add params that we send with the picture
	urlQuery.addQueryItem(QLatin1String("title"), title);
	urlQuery.addQueryItem(QLatin1String("description"), description);

    url.setQuery(urlQuery);
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));

    // If an access token was sent, we upload to account, otherwise we upload
    // anonymously
    if (accessToken.isEmpty()) {
        request.setRawHeader("Authorization", "Client-ID " + mClientId);
    } else {
        request.setRawHeader("Authorization", "Bearer " + accessToken);
    }

    // Post the image
    mAccessManager->post(request, imageByteArray);
}

/*
 * This functions requests an access token, it only starts the request, the
 * topenUpdate signal will be emitted if the request was successful or otherwise
 * the tokenError.
 */
void ImgurWrapper::getAccessToken(const QByteArray& pin, const QByteArray& clientId, const QByteArray& clientSecret) const
{
    QNetworkRequest request;

    // Build the URL that we will request the token from. The XML indicates we
    // want the response in XML format.
    request.setUrl(QUrl(mBaseImgutUrl + QLatin1String("/oauth2/token.xml")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));

    // Prepare the params that we send with the request
    QByteArray params;
    params.append("client_id=" + clientId);
    params.append("&client_secret=" + clientSecret);
    params.append("&grant_type=pin");
    params.append("&pin=" + pin);

    // Request the token
    mAccessManager->post(request, params);
}

/*
 * The imgur token expires after some time, when this happens and you try to
 * post an image the server responds with 403 and we emit the
 * tokenRefreshRequired signal, after which this function should be called to
 * refresh the token.
 */
void ImgurWrapper::refreshToken(const QByteArray& refreshToken, const QByteArray& clientId, const QByteArray& clientSecret) const
{
    QNetworkRequest request;

    // Build the URL that we will request the token from. The XML indicates we
    // want the response in XML format
    request.setUrl(QUrl(mBaseImgutUrl + QLatin1String("/oauth2/token.xml")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));

    // Prepare the params
    QByteArray params;
    params.append("refresh_token=" + refreshToken);
    params.append("&client_id=" + clientId);
    params.append("&client_secret=" + clientSecret);
    params.append("&grant_type=refresh_token");

    // Request the token
    mAccessManager->post(request, params);
}

/*
 * Returns a URL that can be opened in a browser to request the pin. The
 * function is not opening the pin window in the browser, it only returns the
 * correct url to it.
 */
QUrl ImgurWrapper::pinRequestUrl(const QString& clientId) const
{
    QUrl url(mBaseImgutUrl + QLatin1String("/oauth2/authorize"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QLatin1String("client_id"), clientId);
    urlQuery.addQueryItem(QLatin1String("response_type"), QLatin1String("pin"));

    url.setQuery(urlQuery);
    return url;
}

//
// Private Functions
//

/*
 * This function handles the default response, a 200OK and any error message is
 * returned in a data root element. 200OK is returned when posting an image was
 * successful, 403 is mostly returned when a token has expired, anything else is
 * an error that we currently cannot handle
 */
void ImgurWrapper::handleDataResponse(const QDomElement& element) const
{
    if (element.attribute(QLatin1String("status")) == QLatin1String("200") && !element.elementsByTagName(QLatin1String("link")).isEmpty()) {
        auto link = element.elementsByTagName(QLatin1String("link")).at(0).toElement().text();
        auto deleteHash = element.elementsByTagName(QLatin1String("deletehash")).at(0).toElement().text();

        emit uploadFinished(ImgurResponse(link, deleteHash));
    } else if (element.attribute(QLatin1String("status")) == QLatin1String("403")) {
        emit tokenRefreshRequired();
    } else {
        if (element.elementsByTagName(QLatin1String("error")).isEmpty()) {
            emit error(QLatin1String("Server responded with ") + element.attribute(QLatin1String("status")));
        } else {
            emit error(QLatin1String("Server responded with ") + element.attribute(QLatin1String("status")) + ": " +
                       element.elementsByTagName(QLatin1String("error")).at(0).toElement().text());
        }
    }
}

/*
 * Called when a new token was received, either when first time getting access
 * or refreshing a token
 */
void ImgurWrapper::handleTokenResponse(const QDomElement& element) const
{
    if (!element.elementsByTagName(QLatin1String("access_token")).isEmpty() &&
            !element.elementsByTagName(QLatin1String("refresh_token")).isEmpty() &&
            !element.elementsByTagName(QLatin1String("account_username")).isEmpty()
       ) {
        emit tokenUpdated(element.elementsByTagName(QLatin1String("access_token")).at(0).toElement().text(),
                          element.elementsByTagName(QLatin1String("refresh_token")).at(0).toElement().text(),
                          element.elementsByTagName(QLatin1String("account_username")).at(0).toElement().text()
                         );
    } else {
        emit error(QLatin1String("Expected token response was received, something went wrong."));
    }
}

//
// Private Slots
//

/*
 * This function will be called when we've got the reply from imgur
 */
void ImgurWrapper::handleReply(QNetworkReply* reply)
{
    // Only for troubleshooting, if reply->readAll is called the parser will fail!
//    qDebug("----------------------------------------------------------------");
//    qDebug("Reply code:\n%s", qPrintable(reply->readAll()));
//    qDebug("----------------------------------------------------------------");

    // Check network return code, if we get no error or if we get a status 202,
    // proceed, the 202 is returned for invalid token, we will request a new
    // token.
    if (reply->error() != QNetworkReply::NoError &&
            reply->error() != QNetworkReply::ContentOperationNotPermittedError) {
        emit error(QLatin1String("Network Error(") + QString::number(reply->error()) + "): " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QDomDocument doc;
    QString errorMessage;
    int errorLine;
    int errorColumn;

    // Try to parse reply into xml reader
    if (!doc.setContent(reply->readAll(), false, &errorMessage, &errorLine, &errorColumn)) {
        emit error(QLatin1String("Parse error: ") + errorMessage + QLatin1String(", line:") + errorLine +
                   QLatin1String(", column:") + errorColumn);
        reply->deleteLater();
        return;
    }

    // See if we have an upload reply, token response or error
    auto rootElement = doc.documentElement();

    if (rootElement.tagName() == QLatin1String("data")) {
        handleDataResponse(rootElement);
    } else if (rootElement.tagName() == QLatin1String("response")) {
        handleTokenResponse(rootElement);
    }

    else {
        emit error(QLatin1String("Received unexpected reply from imgur server."));
    }

    reply->deleteLater();
}
