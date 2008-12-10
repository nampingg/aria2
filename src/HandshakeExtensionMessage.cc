/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "HandshakeExtensionMessage.h"
#include "Peer.h"
#include "BtContext.h"
#include "Util.h"
#include "DlAbortEx.h"
#include "LogFactory.h"
#include "Logger.h"
#include "message.h"
#include "StringFormat.h"
#include "bencode.h"

namespace aria2 {

const std::string HandshakeExtensionMessage::EXTENSION_NAME = "handshake";

HandshakeExtensionMessage::HandshakeExtensionMessage():
  _tcpPort(0),
  _logger(LogFactory::getInstance()) {}

HandshakeExtensionMessage::~HandshakeExtensionMessage() {}

std::string HandshakeExtensionMessage::getBencodedData()
{
  bencode::BDE dict = bencode::BDE::dict();
  if(!_clientVersion.empty()) {
    dict["v"] = _clientVersion;
  }
  if(_tcpPort > 0) {
    dict["p"] = _tcpPort;
  }
  bencode::BDE extDict = bencode::BDE::dict();
  for(std::map<std::string, uint8_t>::const_iterator itr = _extensions.begin();
      itr != _extensions.end(); ++itr) {
    const std::map<std::string, uint8_t>::value_type& vt = *itr;
    extDict[vt.first] = vt.second;
  }
  dict["m"] = extDict;
  return bencode::encode(dict);
}

std::string HandshakeExtensionMessage::toString() const
{
  std::string s = getExtensionName();
  if(!_clientVersion.empty()) {
    s += " client="+Util::urlencode(_clientVersion);
  }
  if(_tcpPort > 0) {
    s += ", tcpPort="+Util::uitos(_tcpPort);
  }
  for(std::map<std::string, uint8_t>::const_iterator itr = _extensions.begin();
      itr != _extensions.end(); ++itr) {
    const std::map<std::string, uint8_t>::value_type& vt = *itr;
    s += ", "+vt.first+"="+Util::uitos(vt.second);
  }
  return s;
}

void HandshakeExtensionMessage::doReceivedAction()
{
  if(_tcpPort > 0) {
    _peer->port = _tcpPort;
    _peer->setIncomingPeer(false);
  }
  for(std::map<std::string, uint8_t>::const_iterator itr = _extensions.begin();
      itr != _extensions.end(); ++itr) {
    const std::map<std::string, uint8_t>::value_type& vt = *itr;
    _peer->setExtension(vt.first, vt.second);
  }
}

void HandshakeExtensionMessage::setPeer(const PeerHandle& peer)
{
  _peer = peer;
}

void HandshakeExtensionMessage::setBtContext(const BtContextHandle& btContext)
{
  _btContext = btContext;
}

uint8_t HandshakeExtensionMessage::getExtensionMessageID(const std::string& name) const
{
  std::map<std::string, uint8_t>::const_iterator i = _extensions.find(name);
  if(i == _extensions.end()) {
    return 0;
  } else {
    return (*i).second;
  }
}

HandshakeExtensionMessageHandle
HandshakeExtensionMessage::create(const unsigned char* data, size_t length)
{
  if(length < 1) {
    throw DlAbortEx
      (StringFormat(MSG_TOO_SMALL_PAYLOAD_SIZE,
		    EXTENSION_NAME.c_str(), length).str());
  }
  HandshakeExtensionMessageHandle msg(new HandshakeExtensionMessage());
  msg->_logger->debug("Creating HandshakeExtensionMessage from %s",
		      Util::urlencode(data, length).c_str());
  const bencode::BDE dict = bencode::decode(data+1, length-1);
  if(!dict.isDict()) {
    throw DlAbortEx("Unexpected payload format for extended message handshake");
  }
  const bencode::BDE& port = dict["p"];
  if(port.isInteger() && 0 < port.i() && port.i() < 65536) {
    msg->_tcpPort = port.i();
  }
  const bencode::BDE& version = dict["v"];
  if(version.isString()) {
    msg->_clientVersion = version.s();
  }
  const bencode::BDE& extDict = dict["m"];
  if(extDict.isDict()) {
    for(bencode::BDE::Dict::const_iterator i = extDict.dictBegin();
	i != extDict.dictEnd(); ++i) {
      if((*i).second.isInteger()) {
	msg->_extensions[(*i).first] = (*i).second.i();
      }
    }
  }
  return msg;
}

} // namespace aria2
