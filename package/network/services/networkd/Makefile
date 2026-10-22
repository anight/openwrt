#
# networkd: the access control of this router (its own service; source and
# documents in the flint4 tree next to this one: networkd/, NETWORKD.md).
# Not published: the package builds the networkd tree that is checked out
# there, as networkd/dist/deploy.sh does. A new commit there is a new
# version; changes not committed need: make package/networkd/clean
#

include $(TOPDIR)/rules.mk

PKG_NAME:=networkd
NETWORKD_SRC?=$(TOPDIR)/../networkd
NETWORKD_COMMIT:=$(shell git -C $(NETWORKD_SRC) log -1 --format=%h -- . 2>/dev/null)
NETWORKD_DATE:=$(shell git -C $(NETWORKD_SRC) log -1 --format=%cd --date=format:%Y.%m.%d -- . 2>/dev/null)
# what the binaries say (networkd --version, nwd-ap version): as deploy.sh
NETWORKD_BUILD:=$(shell cd $(NETWORKD_SRC) && v=$$(git describe --always 2>/dev/null || echo dev); git diff --quiet HEAD -- . 2>/dev/null || v="$$v-dirty"; echo $$v)
PKG_VERSION:=$(NETWORKD_DATE)~$(NETWORKD_COMMIT)
PKG_RELEASE:=1

PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
# built stripped (-s -w); nwd-ap is for another architecture than strip's
RSTRIP:=:

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/packages/lang/golang/golang-values.mk

define Package/networkd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=networkd: the router's access control (clients, Wi-Fi, switches, units)
  DEPENDS:=@(aarch64) +ucode +ucode-mod-ubus
endef

define Package/networkd/description
  One service that makes every client's LAN, firewall, DHCP and Wi-Fi VLAN,
  manages the remote access point and the switches, and serves the web UIs;
  netcli is its command-line client. It carries nwd-ap, the helper of the
  remote access points (mipsel), which it installs there on request.
endef

GO_HOST_BIN:=$(STAGING_DIR_HOSTPKG)/lib/go-$(GO_HOST_VERSION)/bin/go
GO_BUILD_ENV:=GOTOOLCHAIN=local GOFLAGS=-mod=vendor GOCACHE=$(PKG_BUILD_DIR)/.cache GOPATH=$(PKG_BUILD_DIR)/.gopath \
	GOENV=off CGO_ENABLED=0 GOOS=linux
GO_BUILD_FLAGS:=-trimpath -ldflags "-s -w -X main.version=$(NETWORKD_BUILD)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(NETWORKD_SRC)/. $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	cd $(PKG_BUILD_DIR) && $(GO_BUILD_ENV) GOARCH=$(GO_ARCH) \
		$(GO_HOST_BIN) build $(GO_BUILD_FLAGS) -o bin/networkd ./cmd/networkd
	cd $(PKG_BUILD_DIR) && $(GO_BUILD_ENV) GOARCH=mipsle GOMIPS=softfloat \
		$(GO_HOST_BIN) build $(GO_BUILD_FLAGS) -o bin/nwd-ap.mipsle ./cmd/nwd-ap
endef

define Package/networkd/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/libexec/networkd $(1)/etc/init.d $(1)/etc/hotplug.d/ntp \
		$(1)/lib/upgrade/keep.d $(1)/usr/share/networkd
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/networkd $(1)/usr/sbin/networkd
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dist/netcli $(1)/usr/sbin/netcli
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dist/run $(1)/usr/libexec/networkd/run
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dist/access-limit $(1)/usr/libexec/access-limit
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dist/networkd.init $(1)/etc/init.d/networkd
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/dist/hotplug-ntp $(1)/etc/hotplug.d/ntp/20-networkd
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/dist/keep.d $(1)/lib/upgrade/keep.d/networkd
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/bin/nwd-ap.mipsle $(1)/usr/share/networkd/nwd-ap.mipsle
endef

$(eval $(call BuildPackage,networkd))
