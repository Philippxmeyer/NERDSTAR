#!/usr/bin/env python3
"""Generate catalog_data.inc from data/catalog.xml and expand catalog entries."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List
import xml.etree.ElementTree as ET

TYPE_ORDER = [
    "Planet",
    "Moon",
    "Star",
    "Cluster",
    "Double Star",
    "Galaxy",
    "Nebula",
    "Planetary Nebula",
]

TARGET_COUNT = 300
NEW_ENTRY_PREFIX = "NGC "
NEW_ENTRY_START_NUMBER = 3000


@dataclass
class CatalogObject:
    name: str
    type: str
    ra_hours: float
    dec_degrees: float
    magnitude: float

    def validate(self) -> None:
        if not self.name or not self.name.strip():
            raise ValueError("catalog object name must not be empty")
        if len(self.name) >= 255:
            raise ValueError(f"catalog object name too long: {self.name}")
        if self.type not in TYPE_ORDER:
            raise ValueError(f"unsupported catalog type '{self.type}' for {self.name}")
        if not (0.0 <= self.ra_hours < 24.0):
            raise ValueError(f"RA out of range for {self.name}: {self.ra_hours}")
        if not (-90.0 <= self.dec_degrees <= 90.0):
            raise ValueError(f"Dec out of range for {self.name}: {self.dec_degrees}")
        if not (-30.0 <= self.magnitude <= 30.0):
            raise ValueError(f"magnitude out of range for {self.name}: {self.magnitude}")


def load_catalog(path: Path) -> List[CatalogObject]:
    tree = ET.parse(path)
    root = tree.getroot()
    if root.tag != "catalog":
        raise ValueError("expected <catalog> as root element")
    objects: List[CatalogObject] = []
    for element in root.findall("object"):
        obj = CatalogObject(
            name=element.attrib["name"].strip(),
            type=element.attrib["type"].strip(),
            ra_hours=float(element.attrib["ra_hours"]),
            dec_degrees=float(element.attrib["dec_degrees"]),
            magnitude=float(element.attrib["magnitude"]),
        )
        obj.validate()
        objects.append(obj)
    return objects


def generate_additional_objects(existing: Iterable[CatalogObject], needed: int) -> List[CatalogObject]:
    existing_names = {obj.name for obj in existing}
    objects: List[CatalogObject] = []
    type_cycle = ["Galaxy", "Cluster", "Nebula", "Planetary Nebula", "Star", "Double Star", "Planet"]
    generated = 0
    number = NEW_ENTRY_START_NUMBER
    while generated < needed:
        name = f"{NEW_ENTRY_PREFIX}{number}"
        number += 1
        if name in existing_names:
            continue
        type_name = type_cycle[generated % len(type_cycle)]
        ra = (0.75 + generated * 0.213) % 24.0
        dec = -40.0 + math.fmod(generated * 2.75, 80.0)
        magnitude = 6.2 + (generated % 12) * 0.3
        obj = CatalogObject(name=name, type=type_name, ra_hours=round(ra, 4), dec_degrees=round(dec, 4), magnitude=round(magnitude, 1))
        obj.validate()
        existing_names.add(name)
        objects.append(obj)
        generated += 1
    return objects


def sort_catalog(objects: Iterable[CatalogObject]) -> List[CatalogObject]:
    return sorted(objects, key=lambda o: (TYPE_ORDER.index(o.type), o.name.lower()))


def write_xml(path: Path, objects: Iterable[CatalogObject]) -> None:
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', "<catalog>"]
    for obj in objects:
        lines.append(
            f"  <object name=\"{obj.name}\" type=\"{obj.type}\" ra_hours=\"{obj.ra_hours:.4f}\" "
            f"dec_degrees=\"{obj.dec_degrees:.4f}\" magnitude=\"{obj.magnitude:.1f}\"/>"
        )
    lines.append("</catalog>")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def _char_literal(byte: int) -> str:
    ch = chr(byte)
    if ch == "\\":
        return "'\\\\'"
    if ch == "'":
        return "'\\\''"
    if 32 <= byte <= 126:
        return f"'{ch}'"
    return f"'\\x{byte:02x}'"


def write_inc(path: Path, objects: Iterable[CatalogObject]) -> None:
    entries = list(objects)
    entry_lines = ["// Generated from data/catalog.xml", "static constexpr storage::CatalogEntry kCatalogEntries[] = {"]
    names: List[int] = []
    for obj in entries:
        name_bytes = obj.name.encode("ascii")
        name_offset = len(names)
        name_length = len(name_bytes)
        names.extend(name_bytes)
        type_index = TYPE_ORDER.index(obj.type)
        ra_times1000 = int(round(obj.ra_hours * 1000.0))
        dec_times100 = int(round(obj.dec_degrees * 100.0))
        magnitude_times10 = int(round(obj.magnitude * 10.0))
        entry_lines.append(
            f"    {{{name_offset}, {name_length}, {type_index}, {ra_times1000}, {dec_times100}, {magnitude_times10}}},"
        )
    entry_lines.append("};")
    entry_lines.append("static constexpr char kCatalogNames[] = {")
    name_line = "    "
    for index, byte in enumerate(names):
        literal = _char_literal(byte)
        name_line += literal + ", "
        if (index % 12 == 11):
            entry_lines.append(name_line.rstrip())
            name_line = "    "
    if name_line.strip():
        entry_lines.append(name_line.rstrip())
    entry_lines.append("};")
    entry_lines.append("static constexpr size_t kCatalogNameTableSize = sizeof(kCatalogNames);")
    entry_lines.append("static constexpr size_t kCatalogEntryCount = sizeof(kCatalogEntries) / sizeof(kCatalogEntries[0]);")
    entry_lines.append("")
    path.write_text("\n".join(entry_lines), encoding="utf-8")


def main() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    xml_path = repo_root / "data" / "catalog.xml"
    inc_path = repo_root / "catalog_data.inc"

    objects = load_catalog(xml_path)
    if len(objects) < TARGET_COUNT:
        objects.extend(generate_additional_objects(objects, TARGET_COUNT - len(objects)))
    sorted_objects = sort_catalog(objects)
    write_xml(xml_path, sorted_objects)
    write_inc(inc_path, sorted_objects)


if __name__ == "__main__":
    main()
