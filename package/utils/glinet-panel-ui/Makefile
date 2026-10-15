include $(TOPDIR)/rules.mk

PKG_NAME:=glinet-panel-ui
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/blogic/glinet-panel-ui.git
PKG_MIRROR_HASH:=1340b64eccf3a4c39255baa5579761afed5f29f8384eb58f6aaa5dffa3c07592
PKG_SOURCE_DATE:=2026-08-19
PKG_SOURCE_VERSION:=657c987dc43f152db3b87615823b8ab5a54d3a50

PKG_LICENSE:=GPL-2.0-only OFL-1.1
PKG_LICENSE_FILES:=LICENSE.txt assets/fonts/OFL.txt
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/glinet-panel-ui
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=GL.iNet TFT panel UI
  DEPENDS:=+ucode-mod-lvgl +ucode +ucode-mod-ubus +ucode-mod-uloop \
	+ucode-mod-uci +ucode-mod-fs +ucode-mod-math +ucode-mod-nl80211
endef

define Package/glinet-panel-ui/description
A lvgl based UI for TFT displays on the GL.iNet BE10000 and BE14000
endef

define Build/Compile
endef

define Package/glinet-panel-ui/conffiles
/etc/config/glinet_panel
endef

define Package/glinet-panel-ui/install
	$(INSTALL_DIR) $(1)/usr/share/ucode/glinet-panel-ui/templates $(1)/usr/share/glinet-panel-ui
	$(CP) $(PKG_BUILD_DIR)/ucode/. $(1)/usr/share/ucode/glinet-panel-ui/
	$(CP) $(PKG_BUILD_DIR)/assets/. $(1)/usr/share/glinet-panel-ui/
	$(CP) ./files/. $(1)/
endef

$(eval $(call BuildPackage,glinet-panel-ui))
