# Mantle scripting API for Python. GENERATED from the engine's API registry; do not edit.
from typing import Any, Callable, Optional

class Vector3:
    x: float
    y: float
    z: float
    magnitude: float
    unit: 'Vector3'
    zero: 'Vector3'
    one: 'Vector3'
    x_axis: 'Vector3'
    y_axis: 'Vector3'
    z_axis: 'Vector3'
    def __init__(self, x: float = 0, y: float = 0, z: float = 0) -> None: ...
    def __add__(self, other: 'Vector3 | float') -> 'Vector3': ...
    def __sub__(self, other: 'Vector3 | float') -> 'Vector3': ...
    def __mul__(self, other: 'Vector3 | float') -> 'Vector3': ...
    def __truediv__(self, other: 'Vector3 | float') -> 'Vector3': ...
    def __neg__(self) -> 'Vector3': ...
    def dot(self, other: 'Vector3') -> float: ...
    def cross(self, other: 'Vector3') -> 'Vector3': ...
    def lerp(self, goal: 'Vector3', alpha: float) -> 'Vector3': ...
    def distance(self, other: 'Vector3') -> float: ...

class WaitToken:
    """Yield this from a generator function to wait: `yield task.wait(1)`."""
    result: Any

class Signal:
    def connect(self, handler: Callable[..., Any]) -> 'Connection': ...
    def once(self, handler: Callable[..., Any]) -> 'Connection': ...
    def wait(self) -> WaitToken: ...
    def fire(self, *args: Any) -> None: ...

class Entity:
    """A thing in the world: any actor in the level."""
    anchored: bool
    angular_velocity: Vector3
    can_collide: bool
    clip_time: Optional[float]
    color: str
    exists: bool
    light_color: str
    light_enabled: bool
    light_inner_cone: float
    light_intensity: float
    light_kind: Optional[str]
    light_outer_cone: float
    light_range: float
    mass: float
    name: str
    physics_material: Optional[str]
    position: Vector3
    rotation: Vector3
    scale: Vector3
    sound_id: Optional[str]
    sound_volume: float
    transparency: float
    velocity: Vector3
    visible: bool
    attribute_changed: Signal
    collided: Signal
    touch_ended: Signal
    touched: Signal
    def add_tag(self, tag: str) -> None:
        """Give the entity a tag (fires World:OnTagged handlers)."""
        ...
    def apply_angular_impulse(self, impulse: Vector3) -> None:
        """Spin a physics entity: an angular impulse in kg*cm^2/s about the world axes, in degrees. Does nothing while it is Anchored. Errors on an entity with no physics block."""
        ...
    def apply_impulse(self, impulse: Vector3) -> None:
        """Push a physics entity: impulse is in kg*cm/s along the world axes, so its velocity changes by impulse / Mass. Does nothing while it is Anchored. Errors on an entity with no physics block."""
        ...
    def clone(self) -> Entity:
        """Make a copy (same look, tags and place). On the server the copy appears for everyone; on a client only for that player. Tip: keep a hidden template and clone it. A physics entity, a Constraint or an NPC can't be cloned (it errors, and says how to make another)."""
        ...
    def destroy(self) -> None:
        """Remove the entity. On the server it disappears for everyone; on a client only for that player."""
        ...
    def get_attribute(self, name: str) -> Any:
        """A custom value stored on the entity, or nil."""
        ...
    def get_material(self, slot: Optional[float] = ...) -> Optional[str]:
        """The material asset id a mesh shows in a slot (0 when left out), or nil when the slot shows the mesh's own material."""
        ...
    def get_tags(self) -> list[str]:
        """All of the entity's tags."""
        ...
    def has_tag(self, tag: str) -> bool:
        """Whether the entity has this tag."""
        ...
    def observe_attribute(self, name: str, handler: Callable[..., Any]) -> Connection:
        """Call handler with the attribute's value now and every time it changes."""
        ...
    def play_clip(self, clipAssetId: Optional[str] = ..., params: Optional[dict[str, Any]] = ...) -> None:
        """Play one of the place's AnimClip assets on a SkeletalMesh, starting now. Leave clipAssetId out (or nil) to restart the clip it plays now, or its own. params: Loop (true by default; false holds the last frame), PlayRate (clip seconds per second, 1 by default, clamped to 0..10) and StartOffset (seconds into the clip to start at, 0 by default, clamped to 0..3600). On the server it plays for everyone, players who join later included, in step on every screen; on a client it only changes that client's view. Errors when the entity is not a SkeletalMesh, the id is not an AnimClip in the place's assets, or the clip was authored on another skeleton (on a dedicated server, which never loads clips, that last one shows on each player's screen as the mesh's resting pose instead)."""
        ...
    def play_effect(self) -> None:
        """Effects only: start emitting. A looping effect already playing keeps going; one that does not loop bursts for a second again. On the server it plays for everyone, players who join later included; on a client it only changes that client's view. Errors when the entity is not an Effect."""
        ...
    def play_sound(self) -> None:
        """Sound emitters only: play the emitter's sound again from the beginning (looping if the emitter loops). From a server script every player hears it."""
        ...
    def remove_tag(self, tag: str) -> None:
        """Remove a tag (fires World:OnUntagged handlers)."""
        ...
    def set_attribute(self, name: str, value: Any) -> None:
        """Store a custom value on the entity (numbers, strings, booleans, Vector3, tables). Server values replicate."""
        ...
    def set_effect_color(self, color: str) -> None:
        """Effects only: tint the particles, '#RRGGBB'; the next particles take it. Replicates like PlayEffect. Errors when the entity is not an Effect or the colour is not '#RRGGBB'."""
        ...
    def set_effect_rate(self, rate: float) -> None:
        """Effects only: how many particles spawn a second, clamped to 0..200; 0 emits nothing while it still plays. Replicates like PlayEffect. Errors when the entity is not an Effect."""
        ...
    def set_material(self, assetId: str, slot: Optional[float] = ...) -> None:
        """Show another of the place's Material assets on this mesh: in one material slot (counting from 0), or in every slot when slot is left out. '' puts the place's own material back. Replicates like Color. Errors when the id is not a Material in the place's assets, the slot doesn't exist, or the entity is not a mesh."""
        ...
    def stop_clip(self) -> None:
        """Stop a SkeletalMesh's clip: it shows its resting pose until the next PlayClip. Replicates like PlayClip. Errors when the entity is not a SkeletalMesh."""
        ...
    def stop_effect(self) -> None:
        """Effects only: stop emitting; the particles already out die out. Replicates like PlayEffect. Errors when the entity is not an Effect."""
        ...
    def stop_sound(self, fadeOut: Optional[float] = ...) -> None:
        """Sound emitters only: stop the emitter's sound, fading it out over fadeOut seconds (0 by default). It stays silent until PlaySound, from a server script for players who join later too."""
        ...
    def tween(self, goal: dict[str, Any], seconds: float, style: Optional[str] = ..., direction: Optional[str] = ...) -> Signal:
        """Smoothly move/rotate/scale to the goal. Since script API 1.8 the goal can also fade a light's LightColor or LightIntensity, or a mesh's Color; those errors like setting them would. style: Linear, Sine, Quad, Cubic, Quart, Quint, Exponential, Circular, Back, Bounce, Elastic. direction: In, Out, InOut. Returns a Completed signal. A Position, Rotation or Scale goal on an entity the place did not mark dynamic is a script error, like setting them."""
        ...

class Player:
    """A connected player."""
    avatar: Optional[str]
    character: Optional[Entity]
    health: Optional[float]
    jump_height: Optional[float]
    max_health: Optional[float]
    name: str
    position: Optional[Vector3]
    user_id: str
    walk_speed: Optional[float]
    wearing: dict[str, Any]
    died: Signal
    health_changed: Signal
    def heal(self, amount: float) -> bool:
        """Raise the character's health by amount, up to MaxHealth. Never brings a dead character back (use Respawn). Returns false when the player has no character on foot, is dead, or is already at full health."""
        ...
    def kick(self, reason: Optional[str] = ...) -> None:
        """Remove the player from the session with a message."""
        ...
    def kill(self) -> bool:
        """Kill the character outright: Died fires with no killer, and the place's respawn rules take over as for any other death. Returns false when the player has no character on foot or is already dead."""
        ...
    def respawn(self, position: Optional[Vector3] = ...) -> bool:
        """Give the player a fresh character with full health right now, alive or dead: at position when given, otherwise at the spawn point the place's respawn mode picks. The old character (or a corpse waiting for its respawn) is removed. Returns false when the player is driving a vehicle (eject them first) or cannot be spawned."""
        ...
    def take_damage(self, amount: float, source: Optional[Player] = ...) -> bool:
        """Hurt the character through the same path weapons use, so the game's damage rules apply. source is the player to credit: if this hit kills, Died names them as the killer. Returns false when the player has no character on foot (respawning or driving), is already dead, or the damage was refused."""
        ...
    def teleport(self, position: Vector3) -> bool:
        """Move the player's character instantly. Always lands exactly at position, even inside solid geometry; returns false only when the player has no character to move right now."""
        ...

class Connection:
    """Returned by Signal:Connect; call Disconnect to stop listening."""
    connected: bool
    def disconnect(self) -> None:
        """Stop calling the handler."""
        ...

class Remote:
    """A named network event between server and clients."""
    name: str
    on_client_event: Signal
    on_server_event: Signal
    def fire_all_clients(self, *args: Any) -> None:
        """Send arguments to every player's OnClientEvent handlers."""
        ...
    def fire_client(self, player: Player, *args: Any) -> None:
        """Send arguments to one player's OnClientEvent handlers."""
        ...
    def fire_server(self, *args: Any) -> None:
        """Send arguments to the server's OnServerEvent handlers."""
        ...

class Constraint:
    """One joint between physics entities, from Constraints:Create or Constraints:FromEntity (server only)."""
    broken: Signal
    def destroy(self) -> None:
        """Remove the joint for good: the entities move freely again, and Broken never fires. A joint a script made also goes when every script restarts."""
        ...

class Crowd:
    """One CrowdSpawner's ambient crowd, from Crowds:Get (server only)."""
    density: float
    enabled: bool
    paused: bool
    seed: float

class NPC:
    """A character NPCs:Spawn made. The server walks and animates it; every player sees it."""
    exists: bool
    health: float
    max_health: float
    position: Vector3
    rotation: Vector3
    walk_speed: float
    died: Signal
    drive_finished: Signal
    health_changed: Signal
    move_completed: Signal
    def destroy(self) -> None:
        """Remove the NPC for everyone. Its walk ends without MoveCompleted, and it no longer counts toward the 50."""
        ...
    def drive_along(self, vehicle: Entity, points: list[Vector3], options: Optional[dict[str, Any]] = ...) -> None:
        """As DriveTo, through points in order (one to 64 positions): the car drives each leg in turn, passing the points on the way without stopping, and ends at the last, which is where it arrives. ArriveRadius is the last point's radius."""
        ...
    def drive_to(self, vehicle: Entity, position: Vector3, options: Optional[dict[str, Any]] = ...) -> None:
        """Put the NPC in the driver's seat of vehicle and drive it to position: the NPC sits in the car for every player, which drives straight at the point, and DriveFinished fires when the drive ends. The NPC must be alive and within 4.5 m of the car with a clear line to the seat; it is the car's driver, so no player gets the Drive prompt or can take the seat, and the car errors if it already has a driver or the NPC is driving another car (StopDriving or ExitVehicle first). It replaces a drive the NPC is on in the same car (that one finishes as 'cancelled'). options: MaxSpeed (centimetres per second, 1400 by default, about 50 km/h; clamped to 100..3000) and ArriveRadius (centimetres, 600 by default; clamped to 100..5000). The NPC drives straight at the point over open ground: there are no roads to follow, and it does not steer round walls (a car that can't get on reverses and tries again, then gives up as 'stuck'). Cars and trucks only, not motorcycles or aircraft. Arrived, the NPC stays in the seat with the car braked until ExitVehicle."""
        ...
    def exit_vehicle(self) -> bool:
        """Get the NPC out of the car it is driving, standing beside it, and free the seat. A drive in progress ends as 'cancelled'. Returns false when the NPC is not in a car, the car is still moving faster than a walk (3 m/s; StopDriving, then try again once it has stopped) or there is nowhere to stand beside it."""
        ...
    def get_entity(self) -> Entity:
        """The NPC as an Entity: what a raycast that hits it reports. Tag it or give it attributes like any entity."""
        ...
    def heal(self, amount: float) -> bool:
        """Raise the NPC's health by amount, up to MaxHealth. Never brings a dead NPC back (use Respawn). Returns false when the NPC is dead or already at full health."""
        ...
    def kill(self) -> bool:
        """Kill the NPC outright: Died fires with no killer. Returns false when it is already dead."""
        ...
    def move_to(self, position: Vector3) -> bool:
        """Walk to position along the ground, around walls and obstacles, replacing any walk in progress. Returns false at once when position is more than 500 m away or the NPC is dead; otherwise true, and MoveCompleted fires when the walk ends: true when the NPC arrived, false when there is no way there (no walkable ground, a blocked or too long path). The first walk in a place waits for its navigation to build, which can take a few seconds."""
        ...
    def play_asset_clip(self, clipAssetId: str, params: Optional[dict[str, Any]] = ...) -> None:
        """Play one of the place's AnimClip assets on the NPC, starting now, on every player's screen and in step, players who join later included. params: Loop (false by default: the clip plays once and the NPC goes back to walking and standing on its own; true keeps it playing until StopClip or the next PlayClip), PlayRate (clip seconds per second, 1 by default, clamped to 0..10) and StartOffset (seconds into the clip to start at, 0 by default, clamped to 0..3600). The clip must be authored on the skeleton of the NPC's body (the avatars' MetaHuman body). Errors when the id is not an AnimClip in the place's assets or, where the server draws the NPC, the clip is on another skeleton; a dedicated server never loads clips, so it cannot check the skeleton: there a clip on another skeleton shows on each player's screen as the NPC walking and standing on its own instead, with no error for the script."""
        ...
    def play_clip(self, clip: str, loop: Optional[bool] = ...) -> None:
        """Play one of the NPC clips on every player's screen: 'walk', 'idle', 'turn_left' or 'turn_right', or (script API 1.23) one of the place's AnimClip asset ids at normal speed (PlayAssetClip also sets its speed and start). It plays once and the NPC goes back to walking and standing on its own, or with loop true keeps playing until StopClip or the next PlayClip. Players who join later see it too. A clip on another skeleton than the NPC's body can't be checked on a dedicated server, which never loads clips; each player's screen then shows the NPC walking and standing on its own instead."""
        ...
    def respawn(self, position: Optional[Vector3] = ...) -> bool:
        """Bring the NPC back with full health, dead or alive: standing at position (its feet) when given, otherwise where NPCs:Spawn first put it. Its clip stops, and a walk in progress ends without MoveCompleted. Returns false when it could not be put there."""
        ...
    def stop(self) -> None:
        """Stop walking where the NPC is. The walk it abandons never fires MoveCompleted."""
        ...
    def stop_clip(self) -> None:
        """End the clip PlayClip started: the NPC walks and stands on its own again."""
        ...
    def stop_driving(self) -> None:
        """End the drive the NPC is on: the car brakes to a stop with the NPC still in the seat, and DriveFinished fires with 'cancelled'. Does nothing when the NPC is not driving."""
        ...
    def take_damage(self, amount: float, source: Optional[Player] = ...) -> bool:
        """Hurt the NPC through the same path weapons use. source is the player to credit: if this hit kills, Died names them as the killer. Returns false when the NPC is already dead or the damage was refused."""
        ...

class OrderedDataStore:
    """A named store of whole-number values for this place, read back sorted: a leaderboard. Get one with DataStore:GetOrderedDataStore (server only). Every method returns a Signal that fires once with a result table {ok, error?, ...}: call :Wait() on it straight away, or :Once(handler)."""
    def get_async(self, key: str) -> Signal:
        """Read key's value: the result's value, nil when the store has no such key."""
        ...
    def get_sorted_async(self, ascending: bool, pageSize: Optional[float] = ..., cursor: Optional[str] = ...) -> Signal:
        """One page of the store in value order, highest first (lowest first when ascending): the result's entries is a list of {key, value}, up to pageSize (default 50, at most 100), ties in key order. Pass the result's nextCursor back, with the same ascending, for the next page; it is nil on the last one."""
        ...
    def increment_async(self, key: str, delta: Optional[float] = ...) -> Signal:
        """Add delta (default 1; may be negative) to key's value, a new key starting from 0. One step on the backend, so no server's increment is lost to another's. The result's value is the new total."""
        ...
    def remove_async(self, key: str) -> Signal:
        """Delete key from the store. Removing a key the store doesn't have succeeds."""
        ...
    def set_async(self, key: str, value: float) -> Signal:
        """Store a whole number (up to 2^53 - 1 either way) under key. The result's value is what was stored."""
        ...

class SoundHandle:
    """One sound Sounds:Play started. A server script's handle reaches every player who hears the sound."""
    def is_playing(self) -> bool:
        """Whether the sound is still playing (or still downloading, to play when it arrives). False once it is stopped, or a one-shot has finished. A dedicated server never downloads sounds, so there a one-shot counts as playing for up to a minute unless it is stopped."""
        ...
    def set_volume(self, volume: float) -> None:
        """Change the sound's volume, 0 to 2. From a server script, players hear the change within a tenth of a second."""
        ...
    def stop(self, fadeOut: Optional[float] = ...) -> None:
        """Stop the sound, fading it out over fadeOut seconds (0, the default, stops it at once). Stopping a sound that has already finished does nothing."""
        ...

class UiDocument:
    """A UI document from the place's UI/ folder, open on the local player's screen (UI:OpenDocument). It belongs to the script that opened it and closes when that script stops."""
    is_open: bool
    closed: Signal
    event: Signal
    def close(self) -> None:
        """Close the document. Closed does not fire for your own Close, and closing twice is fine. Example: shop:Close()"""
        ...
    def set_data(self, key: str, value: Any) -> None:
        """Set a value the document shows through {{key}}, data-for, data-if and data-attr-*: a number, string, boolean, Vector3 (read as .x, .y and .z), or a table of them. An entity, a player or any other object reference is refused: send its fields instead. The key is a name of letters, digits and _ (up to 64). After the document closes it still checks the key and value, then does nothing. Example: shop:SetData('coins', 120)"""
        ...
    def set_mode(self, mode: str) -> None:
        """Switch how the document takes input: 'modal', 'overlay' or 'hud' (see UI:OpenDocument). Does nothing once it has closed. Example: shop:SetMode('hud')"""
        ...

class UiNode:
    """One node in a script's retained UI tree, mounted into the local player's HUD."""
    id: float
    text: str
    activated: Signal
    def destroy(self) -> None:
        """Remove this node and every descendant."""
        ...
    def set_action(self, action: str) -> bool:
        """Name the action this node fires on Activated when clicked, tapped or confirmed."""
        ...
    def set_caption(self, caption: str) -> bool:
        """Attach caption text shown underneath this node when the player has captions on."""
        ...
    def set_enabled(self, enabled: bool) -> bool:
        """Enable or disable this node (a disabled button/toggle/prompt cannot be activated)."""
        ...
    def set_image(self, assetId: str) -> bool:
        """Set an image node's asset (an 'ast_' id from the place's asset library)."""
        ...
    def set_localized_text(self, key: str, fallback: Optional[str] = ...) -> bool:
        """Look up key in the place's localisation table for the player's locale (falling back through the language, then the place's default locale, then fallback or key itself) and set it as this node's text."""
        ...
    def set_parent(self, parent: Optional[UiNode] = ...) -> bool:
        """Reparent this node (nil detaches it back to the root). Refuses a cycle or a node from another script."""
        ...
    def set_semantics(self, role: str, label: Optional[str] = ..., hint: Optional[str] = ..., focusOrder: Optional[float] = ...) -> bool:
        """Describe this node for screen readers and controller focus order: 'none', 'heading', 'label', 'button', 'checkbox', 'image', 'list-container', 'list-item', 'dialog' or 'status'."""
        ...
    def set_style(self, style: Optional[dict[str, Any]] = ...) -> bool:
        """Replace this node's style: width, height, columns, font-size; grow (0-100, share of the parent's spare height), gap (0-256, space between children), padding (0-256, space inside the node), opacity (0-1), align ('start', 'center', 'end' or 'stretch', within the parent). Returns false for an unknown key or an out-of-range value; a non-finite number is an error."""
        ...
    def set_text(self, text: str) -> bool:
        """Set this node's literal text."""
        ...
    def set_visible(self, visible: bool) -> bool:
        """Show or hide this node (and its subtree)."""
        ...

class _AssetsService:
    """Spawn the place's own meshes at runtime (server only). Change any mesh's look through its Entity."""
    def spawn(self, assetId: str, position: Vector3, props: Optional[dict[str, Any]] = ...) -> Entity:
        """Spawn one of the place's StaticMesh assets (any mesh its asset list names, placed or not) that every player sees, and return it as an Entity. props sets how it starts: Rotation, Scale (a Vector3, or one number for all three), Material (one of the place's Material assets, for every slot), Color, Transparency, Collision ('Blocking' by default, 'Overlap' or 'None'), CastShadow (true by default) and Anchored (spawned meshes stay where scripts put them; false is refused). A mesh uploaded with the place may take a moment to appear on a player's screen while it downloads. Spawned meshes count towards the same limit as Entity:Clone; Destroy one to make room."""
        ...

Assets: _AssetsService

class _AvatarService:
    """The local player's avatar: open the avatar screen and play their equipped emotes. On the server, read a player's look with Player.Avatar and Player.Wearing."""
    def open_customizer(self) -> bool:
        """Open Mantle's avatar screen, where the player changes their own look. Returns false when there is no local player to open it for. A script cannot change the look itself."""
        ...
    def play_emote(self, itemId: str) -> bool:
        """Play one of the local player's equipped emotes, by its item id (an emote on their emote wheel). Returns false, and plays nothing, when itemId is not one of their equipped emotes, when they have no character, or when called again too soon. true means the request went to the server, which checks it again and can still refuse it (the player is moving, in the air, seated or dead)."""
        ...

Avatar: _AvatarService

class _BackpackService:
    """Give players the built-in Tools and weapons, take them away, and equip them (server only)."""
    def clear(self, player: Player) -> float:
        """Empty the player's Backpack, leaving only fists. Returns how many Tools were removed."""
        ...
    def equip_slot(self, player: Player, slot: float) -> bool:
        """Put the Tool in this Backpack slot in the player's hands. Slots count from 0, so the hotbar key 1 is slot 0. Returns false when the slot is empty or the player is dead."""
        ...
    def get_built_in_tools(self) -> list[str]:
        """The ids GrantBuiltIn accepts in this build: 'grenade', 'healthpack' and 'spawnpad'. 'fists' is never listed. No weapon is built in (editor test sessions also list their test guns)."""
        ...
    def get_tools(self, player: Player) -> list[str]:
        """The ids of the Tools in the player's Backpack, in slot order. Fists, which every player always has, is never listed. Empty while the player has no character (respawning)."""
        ...
    def grant(self, player: Player, item: str) -> str:
        """Give the player a weapon this place stores: item is the library item id Storage:Get returns for one of the place's storage names. Returns the new Tool's stable id. Errors for anything else -- an asset id, a built-in tool's name, an object path, an item the place does not store, or a vehicle. Also errors if the stored weapon has not finished preparing or the player cannot receive it."""
        ...
    def grant_built_in(self, player: Player, tool: str) -> Optional[str]:
        """Give the player one of the built-in Tools: 'grenade', 'healthpack' or 'spawnpad'. Returns the new Tool's id, or nil when the player has no character right now, already has that Tool, or has a full Backpack. A player holding nothing equips it at once. 'fists' is accepted and does nothing: every player always has fists. No weapon is built in: give one from the place's storage with Grant. (Editor test sessions also take the test guns GetBuiltInTools lists there.)"""
        ...
    def remove(self, player: Player, toolId: str) -> bool:
        """Take the Tool with this id out of the player's Backpack (unequipping it first). Returns false when the player does not have it. Fists can never be removed."""
        ...
    def set_weapon_color(self, player: Player, toolId: str, color: str) -> bool:
        """Paint one of the player's weapons: color is '#RRGGBB', the same format as Vehicles:SetPaintColor. Every player sees the new colour. Returns false when the player does not have that Tool or it is not a weapon."""
        ...

Backpack: _BackpackService

class _CameraService:
    """The local player's view (client only): first person, a fixed camera (cut to or tweened into), a camera following an entity, the field of view and a shake. All of it goes back to Mantle's own camera when the script stops or the player respawns."""
    field_of_view: Optional[float]
    mode: str
    def follow(self, entity: Entity, offset: Vector3, relative: Optional[bool] = ...) -> None:
        """Keep the camera at offset from entity, looking at it, wherever it goes (Mode becomes 'Follow'). offset is in world axes, so Vector3.new(0, -600, 300) keeps the camera 6 m along -Y and 3 m up; with relative true it turns with the entity's heading instead, so Vector3.new(-600, 0, 250) stays behind it. The player still moves their character. When the entity is destroyed the camera goes back to Mantle's; set Mode to 'Default' or call Reset to give it back sooner."""
        ...
    def reset(self) -> None:
        """Give the camera back to Mantle: Mode 'Default', Mantle's field of view, no shake."""
        ...
    def set_fixed(self, position: Vector3, lookAt: Vector3) -> None:
        """Hold the camera still at position, looking at lookAt (Mode becomes 'Fixed'). The player still moves their character. Set Mode to 'Default' or call Reset to give the camera back."""
        ...
    def shake(self, intensity: float, duration: float) -> None:
        """Shake the view, fading out over duration seconds. intensity is clamped to 0 to 1 and duration to 0 to 3; a new shake replaces one still running, and 0 stops it. Does nothing when the player turned on Reduced motion."""
        ...
    def tween_fixed(self, position: Vector3, lookAt: Vector3, seconds: float, style: Optional[str] = ..., direction: Optional[str] = ...) -> Signal:
        """Move the camera smoothly from where the view is now to position, looking at lookAt, over seconds (0 to 60), then hold it there like SetFixed. Mode is 'Fixed' from the start. style and direction ease it as they do Entity:Tween (Linear and Out unless given). Returns a signal that fires once when the camera arrives; it never fires if SetFixed, TweenFixed, Follow, Mode or Reset takes the camera first."""
        ...

Camera: _CameraService

class _ChatService:
    """React to what players say in chat, and post lines from the place."""
    message_posted: Signal
    def send_system_message(self, text: str, player: Optional[Player] = ...) -> bool:
        """Post a line from the place into chat, drawn apart from what players say: to everyone, or only to player when you pass one. It goes through the same chat filter as player text, so a line with words that aren't allowed is masked or never shown. At most 500 characters, and at most 10 lines a minute per server. Returns false when it cannot be sent right now: over that limit, the player has no verified account yet, or this server has no session chat (a local test run)."""
        ...

Chat: _ChatService

class _ConstraintsService:
    """Join physics entities with welds, hinges and ropes while the place runs (server only)."""
    def create(self, params: dict[str, Any]) -> Constraint:
        """Join two physics entities, or one to the world, and return the joint. Type is 'weld' (holds them as they are), 'hinge' (EntityA turns about Axis, and only that) or 'rope' (the anchors stay within Length of each other). EntityA is the physics entity the joint holds and EntityB another; leave EntityB out to join EntityA to the world. AnchorA is where on EntityA the joint is, in its own space ((0, 0, 0), its middle, by default); AnchorB where on EntityB (a point in the world without it), by default wherever AnchorA is now. A hinge's Axis is a direction in EntityA's own space ((0, 0, 1) by default) and Limits is {min, max}, the degrees it may turn from where it is now, min at most 0 and max at least 0. A rope's Length is in cm, by default how far apart the anchors are now. Pull the joint harder than BreakForce newtons and it breaks for good (Broken fires). The two entities stop colliding with each other. Players see the entities move; nothing of the joint itself is sent. A place holds at most 200 joints, its own included."""
        ...
    def from_entity(self, entity: Entity) -> Optional[Constraint]:
        """The joint a place's Constraint entity made, as a Constraint, so a script can hear it break or remove it; nil for any other entity."""
        ...

Constraints: _ConstraintsService

class _CrowdsService:
    """Control the ambient crowds a place's CrowdSpawners walk: how many, on or off, moving or frozen (server only). Hear where their people go (client only)."""
    point_reached: Signal
    def get(self, spawner: Entity) -> Crowd:
        """The crowd of a CrowdSpawner the place placed. Every player sees what you set on it. Errors if the entity is not a CrowdSpawner (a clone of one has no crowd) or no longer exists."""
        ...

Crowds: _CrowdsService

class _DataStoreService:
    """Durable saved data for this place (server only)."""
    def get(self, key: str) -> Any:
        """This place's saved value for key, from the local cache (nil if never set, removed, or not loaded yet)."""
        ...
    def get_for_player(self, player: Player, key: str) -> Any:
        """This player's saved value for key, from the local cache."""
        ...
    def get_ordered_data_store(self, name: str) -> OrderedDataStore:
        """The ordered store with this name: whole-number values for this place, read back sorted, for leaderboards. A place can fill 32 stores."""
        ...
    def list_keys(self, prefix: Optional[str] = ..., cursor: Optional[str] = ...) -> dict[str, Any]:
        """One page (up to 100) of this place's saved keys that start with prefix, in key order, from the local cache like Get. Pass nextCursor back for the next page; it is nil on the last one."""
        ...
    def list_keys_for_player(self, player: Player, prefix: Optional[str] = ..., cursor: Optional[str] = ...) -> dict[str, Any]:
        """One page of this player's saved keys, like ListKeys."""
        ...
    def remove_key(self, key: str) -> None:
        """Delete key from this place's saved data. Get reads nil at once; removed from the backend asynchronously, like Set."""
        ...
    def remove_key_for_player(self, player: Player, key: str) -> None:
        """Delete key from this player's saved data, like RemoveKey."""
        ...
    def set(self, key: str, value: Any) -> None:
        """Save key for this place: a boolean, number, string, or table of those (nested up to 4 deep, 64 KB encoded). Applies to the local cache immediately; saved to the backend asynchronously."""
        ...
    def set_for_player(self, player: Player, key: str, value: Any) -> None:
        """Save key scoped to this player, with the same values as Set. Applies to the local cache immediately; saved to the backend asynchronously."""
        ...

DataStore: _DataStoreService

class _EffectsService:
    """Show Mantle's built-in particle effects anywhere in the world: smoke, fire, sparks, dust and glow."""
    def play(self, archetype: str, position: Vector3, params: Optional[dict[str, Any]] = ...) -> None:
        """Show a built-in effect at position for a moment: archetype is 'Smoke', 'Fire', 'Sparks', 'Dust' or 'Glow' (any case). It emits for params.Duration seconds (1 by default, 0 to 10) and is gone once its last particles are. params tune its look, each the archetype's own by default: Color ('#RRGGBB'), Size (particle size, 1 to 1000 cm), Rate (particles a second, 0 to 200), Speed (0 to 5000 cm/s), Lifetime (how long each particle lives, 0.05 to 10 seconds); numbers out of range are clamped. Particles leave along +Z, or along Rotation's up (a Vector3 of pitch, yaw and roll). From a client script only this player sees it; from a server script every player does, but not one who joins later. A dedicated server shows nothing. At most 10 plays a frame per script, and a server script at most 10 a second (bursts of 20); past either the script stops."""
        ...

Effects: _EffectsService

class _EnvironmentService:
    """The time of day, the weather, the sky, the fog and the screen's colour, the same for everyone on the server. Everyone can read them; server scripts can change them, and every player sees the change."""
    bloom: Optional[float]
    clock_time: float
    cloud_cover: float
    color_grading: Optional[str]
    color_grading_intensity: float
    color_tint: str
    contrast: float
    day_length: float
    exposure_bias: float
    fog_color: Optional[str]
    fog_density: float
    is_night: bool
    rain_intensity: float
    saturation: float
    stars_visible: Optional[bool]
    sun_intensity_scale: float
    time_of_day: float
    time_scale: float
    vignette: Optional[float]
    weather: str
    lightning_strike: Signal
    property_changed: Signal
    weather_changed: Signal
    def clear_local_look(self) -> None:
        """Take away this player's own look from SetLocalLook (client only): the picture follows the server's values again."""
        ...
    def freeze_clock(self) -> None:
        """Stop the clock where it is: the same as setting TimeScale to 0. Set TimeScale to 1 to start it again."""
        ...
    def resume_natural_weather(self, blendSeconds: Optional[float] = ...) -> None:
        """Let the weather change on its own again after SetWeather, blending back over blendSeconds (0 to 600, 90 when left out)."""
        ...
    def set_cloud_cover(self, cover: Optional[float] = ...) -> None:
        """Cover this much of the sky with cloud, from 0 to 1, whatever the weather; nil gives the weather its own clouds back. The sun dims and the stars fade under heavy cover."""
        ...
    def set_local_look(self, look: dict[str, Any]) -> None:
        """Change how the picture looks for this player only (client only). look holds any of ExposureBias (-4 to 4), Saturation (0 to 2), Contrast (0 to 2), Vignette (0 to 1), ColorTint ('#RRGGBB'), Bloom (0 to 4), ColorGrading ('Warm', 'Cool', 'Noir', 'Sepia', 'Vivid' or 'Faded') and ColorGradingIntensity (0 to 1), taken as the properties of the same names take them. What look sets wins over the server's values; what it leaves out follows them. Each call replaces the last look whole, and the look goes when this script stops. Time and weather stay the same for everyone. From the server, send one player a look with a Remote's FireClient and call this in its OnClientEvent."""
        ...
    def set_weather(self, weather: str, blendSeconds: Optional[float] = ...) -> None:
        """Hold the weather at Clear, PartlyCloudy, Cloudy, Overcast, Foggy, LightRain, Rain, Storm, LightSnow, Snow, Blizzard, DustCalm or DustStorm until ResumeNaturalWeather, changing to it over blendSeconds (0 to 600, 90 when left out). A Storm brings lightning. The snow and dust weathers (LightSnow, Snow, Blizzard, DustCalm, DustStorm) never come on their own."""
        ...

Environment: _EnvironmentService

class _EventsService:
    """Named events that let scripts on the same side talk to each other, in any language."""
    def get(self, name: str) -> Signal:
        """The event with this name; connect to it or Fire it. Doesn't cross the network (use Net:Remote for that)."""
        ...

Events: _EventsService

class _InputService:
    """The local player's keys, mouse buttons and gamepad buttons, and their sticks, triggers and mouse wheel as axes (client only). Mantle's own controls (the menu, chat, voice, movement, the Backpack and Tool keys) can never be bound or read."""
    def bind_action(self, name: str, keys: list[str], callback: Callable[..., Any]) -> None:
        """Call callback when the player presses one of keys (phase 'Began') and when they let go (phase 'Ended'); key is the name of the key that did it. keys lists 1 to 4 names such as {'Q', 'ButtonY'}: letters, '0' to '9', 'F1' to 'F10', 'Tab', 'MouseLeft', 'MouseRight', 'MouseMiddle', 'MouseButton4', 'MouseButton5', 'ButtonA', 'ButtonB', 'ButtonX', 'ButtonY', 'ButtonL1' and 'ButtonR1'. A key Mantle uses for its own controls is an error; GetBindableKeys lists the free ones. Binding a name again replaces that action. The key still does anything Mantle does with it (F and ButtonY also enter and leave cars). Nothing is heard while Mantle's menu or chat is open."""
        ...
    def bind_axis(self, name: str, axes: list[str], callback: Callable[..., Any]) -> None:
        """Call callback when one of axes moves, at most once a frame per axis; axis is the name of the one that moved. axes lists 1 to 4 of 'LeftStickX', 'LeftStickY', 'RightStickX', 'RightStickY' (-1 to 1, right and up positive, 0 inside the stick's dead zone), 'LeftTrigger', 'RightTrigger' (0 to 1) and 'MouseWheel' (the notches scrolled that frame, up positive; heard only on frames it moves). A stick or trigger whose movement is one of Mantle's own controls is an error; GetBindableAxes lists the free ones. A trigger Mantle uses as a button (fire, aim) can still be read here: the button keeps working, and no script can bind it as a key. Binding a name again replaces that binding; axis bindings and BindAction's actions have separate names. The control still does anything Mantle does with it (the mouse wheel also steps Mantle's camera in and out). Everything reads 0 while Mantle's menu or chat is open."""
        ...
    def get_axis(self, axis: str) -> float:
        """Where axis is right now, by the names BindAxis takes, as its callbacks last heard it: 'MouseWheel' reads the notches of the last frame. 0 while Mantle's menu or chat is open. An axis that is part of Mantle's own controls is an error."""
        ...
    def get_bindable_axes(self) -> list[str]:
        """Every axis name this place can bind: the names BindAxis takes, minus the sticks and triggers whose movement Mantle uses for its own controls. A trigger Mantle uses only as a button (fire, aim) is listed."""
        ...
    def get_bindable_keys(self) -> list[str]:
        """Every key name this place can bind: the names BindAction takes, minus Mantle's own controls."""
        ...
    def is_key_down(self, key: str) -> bool:
        """Whether the player is holding key down right now, by the same names BindAction takes. Always false while Mantle's menu or chat is open. A key Mantle uses for its own controls is an error."""
        ...
    def unbind_action(self, name: str) -> bool:
        """Remove the action this script bound under name. Returns false when it has none by that name. A script's actions also go away by themselves when it stops."""
        ...
    def unbind_axis(self, name: str) -> bool:
        """Remove the axis binding this script made under name. Returns false when it has none by that name. A script's axis bindings also go away by themselves when it stops."""
        ...

Input: _InputService

class _InteractionService:
    """Let players walk up to an object and act on it with a key press, and hear when they do. A place can mark an object interactable itself; here a script makes one, changes what players can do to it, and reacts. The server decides everything: change things and listen to Interacted on the server. The client events are for the local player and only for effects such as a sound or a glow."""
    cancelled: Signal
    interacted: Signal
    progress: Signal
    selected: Signal
    started: Signal
    unselected: Signal
    def remove(self, entity: Entity) -> None:
        """Make the entity not interactable any more, for everyone: no prompt, no key. SetOptions makes it interactable again with the options you give."""
        ...
    def set_display(self, entity: Entity, display: dict[str, Any]) -> None:
        """Change how the entity's prompt looks, for everyone. Only the fields you give change. Style is 'Default' (the key and text of each option, stacked), 'Balanced' (a panel with the Title on top and the options under it) or 'Informative' (like Balanced, with the Description beside the options and an accent line); Balanced and Informative need a Title and show as Default without one. Title is up to 80 characters, Description up to 200, and AccentColor is '#RRGGBB'."""
        ...
    def set_enabled(self, entity: Entity, action: str, enabled: bool, part: str = ...) -> None:
        """Turn one option of the entity off or back on, for everyone. action is 'Primary' or 'Secondary', the input the option uses. Turning an option on also ends its cooldown early. An option with AfterUse 'OneShot' that was used stays used. Pass a part's id for an object with several parts; leave it out for the object as a whole."""
        ...
    def set_options(self, entity: Entity, options: dict[str, Any], part: str = ...) -> None:
        """Make the entity interactable and set what players can do to it, for everyone. options is a list of at most two tables, one for each input: Key ('Primary' by default, or 'Secondary'), Text (what the prompt says beside the key, such as 'Open door'; leave it out to show the key alone), HoldSeconds (hold the key this long; 0 is a tap), PressCount (mash the key this many times instead; wins over HoldSeconds), PressDrainPerSecond (how fast a mash's progress drains, 0 to 10), AfterUse ('None', 'DisableUntilEnabled', 'Cooldown' or 'OneShot') and CooldownSeconds (needed above 0 for 'Cooldown'). Pass a part's id to change only that part of an object the place gave several parts; leave it out for the object as a whole. An empty list leaves the object with nothing to do. Example: Interaction:SetOptions(door, {{Key = 'Primary', Text = 'Open door'}})"""
        ...
    def set_range(self, entity: Entity, range: float) -> None:
        """Set how close a player must stand to the entity, in centimetres, for the prompt to show and the key to work. Clamped to 50..5000. A place's default is 250."""
        ...
    def start_cooldown(self, entity: Entity, action: str, seconds: float, part: str = ...) -> None:
        """Make one option of the entity unusable for this many seconds (up to 3600), for everyone, as if it had just been used with AfterUse 'Cooldown'. action is 'Primary' or 'Secondary'. SetEnabled ends it early. Pass a part's id for an object with several parts; leave it out for the object as a whole."""
        ...

Interaction: _InteractionService

class _LeaderboardService:
    """The built-in leaderboard panel: players press Tab (or pick Leaderboard in the game menu) to see the top 10 of one of this place's ordered stores, with each player's name and picture (server only)."""
    def clear(self) -> None:
        """Take the leaderboard panel down: players no longer see it until Configure shows a board again. The store keeps its entries."""
        ...
    def configure(self, store: str, options: Optional[dict[str, Any]] = ...) -> None:
        """Show the ordered store with this name (as DataStore:GetOrderedDataStore takes it) on the leaderboard panel: its top 10, highest first, or lowest first when options.ascending is true. Key each entry by the player's UserId and the panel shows their name and picture; any other key shows as it is. options.title (up to 32 characters, 'Leaderboard' when left out) heads the panel and options.valueLabel (up to 16, 'Score' when left out) heads the value column. The panel reads the store now and every 30 seconds while anyone is playing, so it trails the store by up to 30 seconds. Call it again to switch boards; one board shows at a time."""
        ...

Leaderboard: _LeaderboardService

class _LightsService:
    """Spawn lights at runtime (server only). Change any light through its Entity's Light members."""
    def spawn(self, kind: str, position: Vector3, props: Optional[dict[str, Any]] = ...) -> Entity:
        """Spawn a light every player sees and return it as an Entity. kind is 'Point', 'Spot' or 'Directional'. props sets how it starts: Color ('#RRGGBB', white by default), Intensity (candelas, 8 by default; lux for a directional light, 10 by default), Range (cm, 1000 by default), InnerCone and OuterCone (degrees, 0 and 44 by default), Enabled, CastShadow (off by default: shadows are costly) and Rotation (a spot light points straight down and a directional light down at 45 degrees unless you give one). Numbers out of range are clamped. At most 64 spawned lights exist at once in a place; Destroy one to make room."""
        ...

Lights: _LightsService

class _NPCsService:
    """Spawn characters the server walks around the place and animates (server only)."""
    def from_entity(self, entity: Entity) -> Optional[NPC]:
        """The NPC an Entity is (a World:Raycast hit, or NPC:GetEntity()), or nil when it is not a live NPC."""
        ...
    def spawn(self, position: Vector3, look: Optional[dict[str, Any]] = ..., yaw: Optional[float] = ...) -> NPC:
        """Spawn a character standing at position (its feet), facing yaw degrees (0 faces +X), and return it as an NPC. Every player sees it. look.AvatarItemId optionally selects a published avatar body; omit it to spawn an NPC with no rendered body. SkinTone and EyeColor are avatar palette ids ('tone_05'), Height is a percent from 95 to 105. A place holds at most 50 NPCs at once; Destroy one to make room."""
        ...

NPCs: _NPCsService

class _NetService:
    """Send messages between the server and players."""
    def remote(self, name: str) -> Remote:
        """Get the remote with this name (the same name on server and client refers to the same remote)."""
        ...

Net: _NetService

class _PlayersService:
    """Everyone in the session."""
    local_player: Player
    player_added: Signal
    player_removing: Signal
    def get_player_from_character(self, character: Entity) -> Optional[Player]:
        """The player whose character this entity is right now, or nil."""
        ...
    def get_players(self) -> list[Player]:
        """Players currently in the session."""
        ...
    def on_player(self, handler: Callable[..., Any]) -> Connection:
        """Call handler for every player: those already here and everyone who joins later. The usual way to set players up."""
        ...

Players: _PlayersService

class _ScriptService:
    """Facts about the running script."""
    is_client: bool
    is_server: bool
    name: str
    package: str
    path: str
    points: list[Entity]
    settings: dict[str, Any]
    side: str

Script: _ScriptService

class _ServerService:
    """Facts about the server this session runs on (server only)."""
    private_server_id: Optional[str]
    private_server_owner_id: Optional[str]

Server: _ServerService

class _SoundsService:
    """Play the sounds uploaded with this place: effects and loops, flat or placed in the world, and one music track per player."""
    def play(self, sound: str, params: Optional[dict[str, Any]] = ...) -> SoundHandle:
        """Play one of the sounds uploaded with this place (a sound emitter's Entity.SoundId, or its id in the place's assets). From a client script only this player hears it; from a server script every player does, or only params.Player. Give Position or AttachTo to place it in the world, where it fades to silence at Radius cm (4000 by default); without either it plays flat, like interface sounds. Volume 0 to 2 (1), Pitch 0.5 to 2 (1), Looped (false), FadeIn seconds (0). A sound that has not finished downloading starts when it arrives. Returns a SoundHandle."""
        ...
    def play_music(self, sound: str, params: Optional[dict[str, Any]] = ...) -> None:
        """Play one of the place's sounds as music: from a client script this player's, from a server script every player's, or only params.Player's. Each player has one music track: a new one crossfades from the old over FadeIn seconds (1 by default), and playing the track already playing only changes its Volume (0 to 2). Loops unless Looped is false."""
        ...
    def stop_music(self, params: Optional[dict[str, Any]] = ...) -> None:
        """Stop the music, fading it out over params.FadeOut seconds (0, the default, stops it at once). From a server script: every player's music, or only params.Player's."""
        ...

Sounds: _SoundsService

class _StateService:
    """Shared game values the server sets and everyone can read (score, round, phase)."""
    changed: Signal
    def get(self, key: str) -> Any:
        """The current value, or nil."""
        ...
    def observe(self, key: str, handler: Callable[..., Any]) -> Connection:
        """Call handler with the value now and every time it changes. The easy way to keep UI in sync."""
        ...
    def set(self, key: str, value: Any) -> None:
        """Set a value for everyone. Numbers, strings, booleans, Vector3 and tables work."""
        ...

State: _StateService

class _StorageService:
    """The library items (vehicles, weapons and script packages) this place stores, by the names its storage gives them."""
    def get(self, name: str) -> str:
        """The library item id (itm_u...) of the entry this place's storage declares under name. The name is case-sensitive. Pass the id to Vehicles:Spawn (a stored vehicle) or Backpack:Grant (a stored weapon): they accept only an id the place stores. Errors when the place's storage has no entry with that name."""
        ...

Storage: _StorageService

class _TimeService:
    """Clocks for scripts."""
    clock: float
    now: float

Time: _TimeService

class _UIService:
    """The local player's on-screen UI: a simple HUD line/toast, and the retained node tree behind real widgets."""
    captions: bool
    high_contrast: bool
    locale: str
    reduced_motion: bool
    text_scale: float
    def action_label(self, action: str) -> str:
        """The player's remapped label for an action name (e.g. the bound key), or the action name itself if unset."""
        ...
    def create(self, kind: str) -> Optional[UiNode]:
        """Create a new node: 'frame', 'text', 'image', 'button', 'toggle', 'list', 'grid', 'scroll' or 'prompt'. Nil means the per-script node budget (256) is exhausted."""
        ...
    def format_number(self, value: float) -> str:
        """Format a number for display using the player's current locale."""
        ...
    def open_document(self, path: str, mode: Optional[str] = ...) -> UiDocument:
        """Open one of the place's UI documents (a .rml file in its UI/ folder) on this player's screen. Mode is 'modal' (the default: gameplay input stops and Back closes it), 'overlay' (play goes on; the mouse works on its controls) or 'hud' (drawn only, never takes input). An unknown path or mode is an error, and so is opening on a server. Example: local shop = UI:OpenDocument('UI/shop.rml')"""
        ...
    def set_cursor_visible(self, visible: Optional[bool] = ...) -> None:
        """Force the mouse cursor on or off regardless of whether an interactive node is visible right now. Nil goes back to automatic: shown only while a Button/Toggle/Prompt is visible, enabled and has an action. A gamepad-only panel can call SetCursorVisible(false) to keep working through focus/Accept without ever drawing a cursor."""
        ...
    def set_text(self, slot: str, text: str) -> None:
        """Show text in a named HUD line (same slot replaces the previous text; empty text clears it)."""
        ...
    def toast(self, text: str, seconds: Optional[float] = ...) -> None:
        """Pop up a short message."""
        ...

UI: _UIService

class _VehiclesService:
    """Spawn and seat cars: the vehicles in the place's storage (server only)."""
    def eject(self, player: Player) -> bool:
        """Put the player back on foot beside the car they are driving. Returns false when they are not driving, the car is still moving, or there is nowhere beside it to stand."""
        ...
    def get_speed(self, vehicle: Entity) -> float:
        """The car's speed in centimetres per second, as the simulation reports it. Errors if the entity is not a car or no longer exists."""
        ...
    def is_locked(self, vehicle: Entity) -> bool:
        """Whether the car is locked. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Errors if the entity is not a car or no longer exists."""
        ...
    def lock(self, vehicle: Entity) -> bool:
        """Lock the car. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Returns false when the entity is not a car."""
        ...
    def seat(self, vehicle: Entity, player: Player) -> bool:
        """Put the player in the driver's seat, through the same checks as walking up and pressing the prompt: the player must be on foot, alive, near the car with a clear line to the seat, and the seat must be empty. Returns false when any of that refuses."""
        ...
    def set_livery(self, vehicle: Entity, index: float) -> bool:
        """Put one of the car's liveries on it: index counts from 0 in the liveries its vehicle was published with, and -1 takes the livery off. Every player sees the change. Returns false when the entity is not a car or the car has no livery at that index (the built-in cars have none)."""
        ...
    def set_paint_color(self, vehicle: Entity, color: str) -> bool:
        """Paint the car: color is '#RRGGBB', the same format as Backpack:SetWeaponColor. Returns false when the entity is not a car."""
        ...
    def spawn(self, item: str, position: Vector3) -> Entity:
        """Spawn a car at position and return it as an Entity. item is the library item id of a vehicle this place stores: the string Storage:Get returns for one of the place's storage names. Any other value errors -- an asset id, an object path, a built-in car's name, an item the place does not store, or a weapon. The car is a real replicated vehicle: every player sees it, walks up to it and can drive it. Read its Position, tag it, or Destroy it like any entity."""
        ...
    def unlock(self, vehicle: Entity) -> bool:
        """Unlock the car, so anyone can open it again. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Returns false when the entity is not a car."""
        ...

Vehicles: _VehiclesService

class _WorldService:
    """Find and watch the things in the level."""
    def blockcast(self, origin: Vector3, size: Vector3, direction: Vector3, params: dict[str, Any] = ...) -> Optional[dict[str, Any]]:
        """Sweep a box of this size (its full width, depth and height, at most 100 m on any side, kept square to the world's axes) from origin along direction and return the first thing it touches, in the same table as World:Raycast, or nil. Takes the same params as World:Raycast."""
        ...
    def find_by_tag(self, tag: str) -> list[Entity]:
        """Every entity with this actor tag."""
        ...
    def find_first(self, name: str) -> Optional[Entity]:
        """The first entity whose name (actor label) matches, or nil."""
        ...
    def get_entities_in_box(self, center: Vector3, size: Vector3, params: dict[str, Any] = ...) -> list[Entity]:
        """Every entity and player character touching a box of this size (its full width, depth and height, at most 100 m on any side, kept square to the world's axes) centred on center, nearest first. Takes the same params as World:GetEntitiesInRadius."""
        ...
    def get_entities_in_radius(self, center: Vector3, radius: float, params: dict[str, Any] = ...) -> list[Entity]:
        """Every entity and player character touching a ball of this radius (at most 50 m) around center, nearest first. Overlap-only entities (triggers) count; entities with no collision never do. A character is listed as its Entity; Players:GetPlayerFromCharacter tells whose it is. params: Include, Exclude, Tag, IgnorePlayers, and MaxResults (256 at most, and by default). Runs on this machine only; a script may make 256 spatial queries a frame."""
        ...
    def on_tagged(self, tag: str, handler: Callable[..., Any]) -> Connection:
        """Call handler for every entity that has the tag now, and for each one that gets it later."""
        ...
    def on_untagged(self, tag: str, handler: Callable[..., Any]) -> Connection:
        """Call handler when an entity loses the tag."""
        ...
    def raycast(self, origin: Vector3, direction: Vector3, params: dict[str, Any] = ...) -> Optional[dict[str, Any]]:
        """Cast a ray from origin along direction and return the first thing it hits: a table of Entity, Player, Position, Normal and Distance, or nil. Player is set when the hit is a player's character. The ray is as long as direction, or params.MaxDistance when given, and at most 1 km. It passes through overlap-only entities (triggers) and never hits entities with no collision. params: MaxDistance, Include (only these entities or players count), Exclude, Tag (only entities with this tag count), IgnorePlayers. Runs on this machine only; a script may make 256 spatial queries a frame."""
        ...
    def spherecast(self, origin: Vector3, radius: float, direction: Vector3, params: dict[str, Any] = ...) -> Optional[dict[str, Any]]:
        """Sweep a ball of this radius (at most 50 m) from origin along direction and return the first thing it touches, in the same table as World:Raycast, or nil. Distance is how far the ball's centre travelled. Takes the same params as World:Raycast."""
        ...

World: _WorldService

class _Task:
    def wait(self, seconds: float = 0) -> WaitToken: ...
    def spawn(self, fn: Callable[..., Any], *args: Any) -> int: ...
    def delay(self, seconds: float, fn: Callable[..., Any], *args: Any) -> int: ...
    def defer(self, fn: Callable[..., Any], *args: Any) -> int: ...
    def cancel(self, id: int) -> None: ...

task: _Task

def warn(*args: Any) -> None: ...
def require(path: str) -> Any: ...
