#
# Homebrew formula file for juise
# https://github.com/mxcl/homebrew
#

require 'formula'

class Juise < Formula
  homepage 'https://github.com/Juniper/juise'
  url 'https://github.com/Juniper/juise/releases/0.8.0/juise-0.8.0.tar.gz'
  sha1 '721b5d34adbdeed942d884aa84c347710fa87efc'

  depends_on 'libtool' => :build
  depends_on 'libslax'
  depends_on 'libssh2'
  depends_on 'pcre'

  # Need newer versions of these libraries
  if MacOS.version <= :lion
    depends_on 'libxml2'
    depends_on 'libxslt'
    depends_on 'curl'
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--with-libssh2-prefix=#{HOMEBREW_PREFIX}"
    system "make install"
  end
end
