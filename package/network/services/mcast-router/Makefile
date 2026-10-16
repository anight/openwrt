#
# mcast-router: policy-based relay of link-local multicast discovery
# (mDNS, SSDP) between networks. Sources in ./src.
#
# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=mcast-router
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=Andrei Nigmatulin <andrei.nigmatulin@gmail.com>

include $(INCLUDE_DIR)/package.mk

define Package/mcast-router
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Policy-based relay for link-local multicast discovery (mDNS, SSDP)
endef

define Package/mcast-router/description
  Relays mDNS and SSDP between networks (one LAN per client) by policy:
  queries go to the networks of the devices the asker may discover,
  answers and announcements to the networks of the devices that may
  discover their sender. Protocols are separate source files built in.
endef

define Package/mcast-router/conffiles
/etc/mcast-router.conf
endef

define Package/mcast-router-probe
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Test tool for mcast-router (send and listen for multicast)
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" all
endef

define Package/mcast-router/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcast-router $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/mcast-router.init $(1)/etc/init.d/mcast-router
	$(INSTALL_CONF) ./files/mcast-router.conf $(1)/etc/
endef

define Package/mcast-router-probe/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcr-probe $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,mcast-router))
$(eval $(call BuildPackage,mcast-router-probe))
