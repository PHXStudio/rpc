package arpc

import (
	"sync"
)

// EnumInfo provides runtime enum metadata.
// It stores information about enum values for serialization/deserialization.
type EnumInfo struct {
	name  string
	items []string
}

// NewEnumInfo creates a new EnumInfo with the given name and item names.
func NewEnumInfo(name string, items []string) *EnumInfo {
	return &EnumInfo{
		name:  name,
		items: items,
	}
}

// Name returns the enum name.
func (e *EnumInfo) Name() string {
	return e.name
}

// Items returns the list of enum item names.
func (e *EnumInfo) Items() []string {
	return e.items
}

// GetItemId returns the ID (index) of an enum item by name.
// Returns -1 if the item is not found.
func (e *EnumInfo) GetItemId(item string) int {
	for i, name := range e.items {
		if name == item {
			return i
		}
	}
	return -1
}

// GetItemName returns the name of an enum item by ID.
// Returns empty string if the ID is out of range.
func (e *EnumInfo) GetItemName(id int) string {
	if id < 0 || id >= len(e.items) {
		return ""
	}
	return e.items[id]
}

// Global enum registry for runtime lookup.
// This map is populated by generated code's init() functions.
var (
	enumRegistry   = make(map[string]*EnumInfo)
	registryMutex sync.RWMutex
)

// RegisterEnum registers an EnumInfo in the global registry.
// This is typically called from generated code's init() function.
func RegisterEnum(name string, info *EnumInfo) {
	registryMutex.Lock()
	defer registryMutex.Unlock()
	enumRegistry[name] = info
}

// GetEnumInfo retrieves EnumInfo from the registry by name.
// Returns (nil, false) if not found.
func GetEnumInfo(name string) (*EnumInfo, bool) {
	registryMutex.RLock()
	defer registryMutex.RUnlock()
	info, ok := enumRegistry[name]
	return info, ok
}

// AllEnumInfos returns a copy of all registered enum infos.
// This is useful for debugging and introspection.
func AllEnumInfos() map[string]*EnumInfo {
	registryMutex.RLock()
	defer registryMutex.RUnlock()

	result := make(map[string]*EnumInfo, len(enumRegistry))
	for name, info := range enumRegistry {
		result[name] = info
	}
	return result
}
