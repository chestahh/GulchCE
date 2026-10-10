package com.halo.decomp;

final class ReleaseVersion {
    static int code(String tag) {
        if (tag == null || !tag.matches("v(?:0|[1-9][0-9]{0,2})\\.(?:0|[1-9][0-9]{0,2})(?:\\.(?:0|[1-9][0-9]{0,2}))?"))
            return -1;
        String[] parts = tag.substring(1).split("\\.");
        return Integer.parseInt(parts[0]) * 1000000 + Integer.parseInt(parts[1]) * 1000
            + (parts.length == 3 ? Integer.parseInt(parts[2]) : 0);
    }
    private ReleaseVersion() {}
}
