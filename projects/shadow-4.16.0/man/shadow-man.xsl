<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:ss="http://docbook.sf.net/xmlns/string.subst/1.0" version="1.0">
  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/profile-docbook.xsl"/>
  <xsl:param name="vendordir"/>

  <xsl:param name="man.string.subst.map.local.pre">
    <ss:substitution oldstring="%vendordir%" newstring="{$vendordir}" />
  </xsl:param>
</xsl:stylesheet>
