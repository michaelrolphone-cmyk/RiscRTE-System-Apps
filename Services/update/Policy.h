#pragma once
/* Product policy is selected at build time, never from downloaded metadata. */
namespace WatchUpdate {
#ifdef UPDATE_PRODUCT_X4
constexpr const char *Repository="michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO";
constexpr const char *Product="xteink-x4-pro";
constexpr const char *AssetPrefix="xteink-x4-pro";
constexpr bool RequireProduct=true,AllowRuntimeOnly=false;
/* No X4 release feed is published. An owner must explicitly configure one. */
#ifndef UPDATE_CATALOG_URL
#define UPDATE_CATALOG_URL ""
#endif
constexpr const char *CatalogUrl=UPDATE_CATALOG_URL;
#else
constexpr const char *Repository="michaelrolphone-cmyk/RiscRTE-T-Watch-S3";
constexpr const char *Product="twatch-s3";
constexpr const char *AssetPrefix="twatch-s3";
constexpr bool RequireProduct=false,AllowRuntimeOnly=true;
constexpr const char *CatalogUrl="https://raw.githubusercontent.com/michaelrolphone-cmyk/RiscRTE-T-Watch-S3/release-index/release-index.json";
#endif
}
